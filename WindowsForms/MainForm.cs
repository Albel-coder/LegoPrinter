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

                ReadConfigButton.Click += LoadConfigButton_Click;
                ExecuteGcodeButton.Click += ExecuteGcodeButton_Click;
                LogTimer.Tick += LogTimer_Tick;

                // Инициализация с обработкой ошибок
                InitializeControllers();

                LogTimer.Start();
                Console.WriteLine("MainForm initialized successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"CRITICAL ERROR in MainForm constructor: {ex}");
                Console.WriteLine($"Failed to initialize application: {ex.Message}");
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
                    Console.WriteLine("Connected successfully!");
                }
                else
                {
                    string error = printerController.GetLastError();
                    Console.WriteLine($"Failed to connect: {error}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in ConnectButton_Click: {ex}");
                Console.WriteLine($"Error with connect: {ex.Message}");
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
                Console.WriteLine("Disconnected successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in DisconnectButton_Click: {ex}");
                Console.WriteLine($"Error disconnecting: {ex.Message}");
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

        private void LoadConfigButton_Click(object sender, EventArgs e)
        {
            try
            {
                bool Success = Interpreter.ReadConfig("Printer.cfg");

                if (Success)
                {
                    Console.WriteLine("Configuration loaded successfully");
                }
                else
                {
                    string error = Interpreter.GetLastError();
                    Console.WriteLine($"Failed to load config: {error}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Config error: {ex.Message}");
            }
        }

        private void ClearLogsButton_Click(object sender, EventArgs e)
        {
            try
            {
                LogTextBox.Clear();
                Interpreter.ClearLog();
                LastLogCount = 0;
                LastInterpreterLogCount = 0;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error clearing logs: {ex.Message}");
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
                    Console.WriteLine($"File '{fullPath}' not found!");
                    return;
                }

                var printerHandle = printerController.GetPrinterHandle();

                Console.WriteLine($"C#: Printer handle: VirtualTable={printerHandle.VirtualTable}");

                if (printerHandle.VirtualTable == IntPtr.Zero)
                {
                    Console.WriteLine("Printer handle is invalid!");
                    return;
                }

                // ПРОВЕРКА СТАТУСА 
                var status = Interpreter.GetStatus();
                Console.WriteLine($"C#: Current interpreter status: {status}");

                if (status == GCodeInterpreter.Status.RUNNING ||
                    status == GCodeInterpreter.Status.PAUSED ||
                    status == GCodeInterpreter.Status.CHECKING_CODE)
                {
                    Console.WriteLine("Interpreter is busy. Please wait for current execution to complete.");
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
                    Console.WriteLine("G-code execution started successfully");
                    StartExecutionMonitoring();
                }
                else
                {
                    string error = Interpreter.GetLastError();
                    Console.WriteLine($"C#: ExecuteFile failed: {error}");
                    Console.WriteLine($"Failed to execute G-code: {error}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"C#: Exception in ExecuteGcodeButton_Click: {ex}");
                Console.WriteLine($"Execution error: {ex.Message}");
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
