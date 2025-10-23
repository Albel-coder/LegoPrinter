using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WindowsForms
{
    public partial class MainForm : Form
    {
        private PrinterController printerController;
        private GCodeInterpreter Interpreter;
        private int LastLogCount = 0;
        private int LastInterpreterLogCount = 0;
        private bool AutoScrollEnabled = true;

        public MainForm()
        {
            try
            {
                Console.WriteLine("Initializing MainForm...");
                InitializeComponent();

                // Инициализация с обработкой ошибок
                InitializeControllers();

                LogTimer.Start();
                Console.WriteLine("MainForm initialized successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"CRITICAL ERROR in MainForm constructor: {ex}");
                MessageBox.Show($"Failed to initialize application: {ex.Message}", "Fatal Error",
                              MessageBoxButtons.OK, MessageBoxIcon.Error);
                throw; // Перебрасываем исключение, чтобы увидеть его в отладчике
            }
        }

        private void InitializeControllers()
        {
            try
            {
                Console.WriteLine("Initializing PrinterController...");
                printerController = new PrinterController();
                Console.WriteLine("PrinterController initialized successfully");

                Console.WriteLine("Initializing GCodeInterpreter...");
                Interpreter = new GCodeInterpreter();
                Console.WriteLine("GCodeInterpreter initialized successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR initializing controllers: {ex}");
                throw;
            }
        }

        private async void ConnectButton_Click(object sender, EventArgs e)
        {
            ConnectButton.Enabled = false;
            ConnectButton.Text = "Connecting...";

            try
            {
                Console.WriteLine("Attempting to connect...");
                bool isConnected = await Task.Run(() => printerController.Connect());

                if (isConnected)
                {
                    MessageBox.Show("Connected successfully!", "Success",
                                  MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    string error = printerController.GetLastError();
                    MessageBox.Show($"Failed to connect: {error}", "Connection Error",
                                  MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in ConnectButton_Click: {ex}");
                MessageBox.Show($"Error with connect: {ex.Message}", "Error",
                              MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                ConnectButton.Enabled = true;
                ConnectButton.Text = "Connect";
            }
        }

        private void DisconnectButton_Click(object sender, EventArgs e)
        {
            try
            {
                Console.WriteLine("Disconnecting...");
                printerController.Disconnect();
                MessageBox.Show("Disconnected successfully", "Disconnected",
                              MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in DisconnectButton_Click: {ex}");
                MessageBox.Show($"Error disconnecting: {ex.Message}", "Error",
                              MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            try
            {
                Console.WriteLine("MainForm is closing, disposing resources...");
                LogTimer.Stop();
                Interpreter?.Dispose();
                printerController?.Dispose();                
                Console.WriteLine("Resources disposed successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR during disposal: {ex}");
            }
            base.OnFormClosing(e);
        }

        private void Test_Click(object sender, EventArgs e)
        {
            try
            {
                Console.WriteLine("Testing interpreter...");
                var printerHandle = printerController.GetPrinterHandle();
                bool success = Interpreter.Test(printerHandle);

                if (success)
                {
                    MessageBox.Show("Interpreter test completed successfully", "Test Result",
                                  MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    string error = Interpreter.GetLastError();
                    MessageBox.Show($"Interpreter test failed: {error}", "Test Result",
                                  MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in Test_Click: {ex}");
                MessageBox.Show($"Test error: {ex.Message}", "Error",
                              MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void MotorTest_Click(object sender, EventArgs e)
        {
            try
            {
                Console.WriteLine("Running motor test...");
                printerController.Test();
                MessageBox.Show("Motor test completed", "Test Result",
                              MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in MotorTest_Click: {ex}");
                MessageBox.Show($"Motor test error: {ex.Message}", "Error",
                              MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void LogTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                UpdateLegoDisplay();
                UpdateStatusDisplay();
                UpdateInterpreterLogDisplay();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in LogTimer_Tick: {ex}");
            }
        }

        private void UpdateLegoDisplay()
        {
            try
            {
                int CurrentLogCount = printerController.GetLogCount();

                if (CurrentLogCount > LastLogCount)
                {
                    for (int i = LastLogCount; i < CurrentLogCount; i++)
                    {
                        string LogEntry = printerController.GetLogEntry(i);
                        if (!string.IsNullOrEmpty(LogEntry))
                        {
                            LogTextBox.AppendText(LogEntry + Environment.NewLine);
                        }
                    }

                    LastLogCount = CurrentLogCount;

                    if (AutoScrollEnabled)
                    {
                        LogTextBox.SelectionStart = LogTextBox.Text.Length;
                        LogTextBox.ScrollToCaret();
                    }
                }
            }
            catch (Exception ex)
            {
                LogTextBox.AppendText($"[UI ERROR] Failed to update log: {ex.Message}\r\n");
            }
        }

        private void UpdateInterpreterLogDisplay()
        {
            try
            {
                int CurrentLogCount = Interpreter.GetLogCount();

                if (CurrentLogCount > LastInterpreterLogCount)
                {
                    for (int i = LastInterpreterLogCount; i < CurrentLogCount; i++)
                    {
                        string LogEntry = Interpreter.GetLog(i);
                        if (!string.IsNullOrEmpty(LogEntry))
                        {
                            InterpreterTexBox.AppendText(LogEntry + Environment.NewLine);
                        }
                    }

                    LastInterpreterLogCount = CurrentLogCount;

                    if (AutoScrollEnabled)
                    {
                        InterpreterTexBox.SelectionStart = InterpreterTexBox.Text.Length;
                        InterpreterTexBox.ScrollToCaret();
                    }
                }
            }
            catch (Exception ex)
            {
                InterpreterTexBox.AppendText($"[UI ERROR] Failed to update interpreter log: {ex.Message}\r\n");
            }
        }

        private void UpdateStatusDisplay()
        {
            try
            {
                StatusLabel.Text = $"Status: {Interpreter.GetStatus()}";
                //ProgressBar.Value = (int)(Interpreter.GetProgress() * 100);

                int errorCount = Interpreter.GetErrorCount();
                //ErrorCountLabel.Text = $"Errors: {errorCount}";

                if (errorCount > 0)
                {
                    //ErrorCountLabel.ForeColor = Color.Red;
                }
                else
                {
                    //ErrorCountLabel.ForeColor = Color.Green;
                }
            }
            catch (Exception ex)
            {
                StatusLabel.Text = $"Status: Error - {ex.Message}";
            }
        }        

        private void LoadConfigButton_Click(object sender, EventArgs e)
        {
            try
            {
                bool Success = Interpreter.ReadConfig("Printer.cfg");

                if (Success)
                {
                    MessageBox.Show("Configuration loaded successfully", "Config",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    string error = Interpreter.GetLastError();
                    MessageBox.Show($"Failed to load config: {error}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Config error: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void ClearLogsButton_Click(object sender, EventArgs e)
        {
            try
            {
                LogTextBox.Clear();
                InterpreterTexBox.Clear();
                Interpreter.ClearLog();
                LastLogCount = 0;
                LastInterpreterLogCount = 0;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error clearing logs: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        private async void ExecuteGcodeButton_Click(object sender, EventArgs e)
        {
            Console.WriteLine("=== ExecuteGcodeButton_Click START ===");
            ExecuteGcodeButton.Enabled = false;
            ExecuteGcodeButton.Text = "Executing...";

            try
            {
                string filename = "G-code.txt";
                string fullPath = Path.GetFullPath(filename);

                Console.WriteLine($"C#: Full path: {fullPath}");
                Console.WriteLine($"C#: File exists: {File.Exists(fullPath)}");

                if (!File.Exists(fullPath))
                {
                    MessageBox.Show($"File '{fullPath}' not found!", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                var printerHandle = printerController.GetPrinterHandle();

                Console.WriteLine($"C#: Printer handle: VirtualTable={printerHandle.VirtualTable}");

                if (printerHandle.VirtualTable == IntPtr.Zero)
                {
                    MessageBox.Show("Printer handle is invalid!", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                // ПРОВЕРКА СТАТУСА 
                var status = Interpreter.GetStatus();
                Console.WriteLine($"C#: Current interpreter status: {status}");

                if (status == GCodeInterpreter.Status.RUNNING ||
                    status == GCodeInterpreter.Status.PAUSED ||
                    status == GCodeInterpreter.Status.CHECKING_CODE)
                {
                    MessageBox.Show("Interpreter is busy. Please wait for current execution to complete.", "Busy",
                        MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                // ДОБАВЛЯЕМ: небольшая задержка для гарантии завершения предыдущего потока
                if (status == GCodeInterpreter.Status.COMPLETED || status == GCodeInterpreter.Status.ERROR)
                {
                    await Task.Delay(200); // 200ms задержка
                }

                // Очищаем ошибки если были
                if (status == GCodeInterpreter.Status.ERROR)
                {
                    Interpreter.ClearErrors();
                }

                Console.WriteLine("C#: Calling Interpreter.ExecuteFile...");
                bool success = await Task.Run(() => Interpreter.ExecuteFile(fullPath, printerHandle));

                Console.WriteLine($"C#: ExecuteFile returned: {success}");

                if (success)
                {
                    MessageBox.Show("G-code execution started successfully", "Execution",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                    StartExecutionMonitoring();
                }
                else
                {
                    string error = Interpreter.GetLastError();
                    Console.WriteLine($"C#: ExecuteFile failed: {error}");
                    MessageBox.Show($"Failed to execute G-code: {error}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"C#: Exception in ExecuteGcodeButton_Click: {ex}");
                MessageBox.Show($"Execution error: {ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                ExecuteGcodeButton.Enabled = true;
                ExecuteGcodeButton.Text = "Execute G-code";
                Console.WriteLine("=== ExecuteGcodeButton_Click END ===");
            }
        }
        private void StartExecutionMonitoring()
        {
            var timer = new System.Windows.Forms.Timer();
            timer.Interval = 1000;
            timer.Tick += (s, e) =>
            {
                var status = Interpreter.GetStatus();
                var progress = Interpreter.GetProgress();

                StatusLabel.Text = $"Status: {status}, Progress: {progress:F1}%";

                if (status == GCodeInterpreter.Status.COMPLETED ||
                    status == GCodeInterpreter.Status.ERROR ||
                    status == GCodeInterpreter.Status.IDLE)
                {
                    timer.Stop();
                    timer.Dispose();
                    UpdateInterpreterLogDisplay();
                }
            };
            timer.Start();
        }
    }
}
