using System;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace WindowsForms
{
    public partial class MainForm : Form
    {
        #region Private Fields
        private PrinterController _printerController;
        private GCodeInterpreter _interpreter;
        private int _lastLogCount = 0;
        private int _lastInterpreterLogCount = 0;
        private bool _autoScrollEnabled = true;
        private System.Windows.Forms.Timer _executionMonitorTimer;
        #endregion

        #region Constructor
        public MainForm()
        {
            try
            {
                Console.WriteLine("Initializing MainForm...");
                InitializeComponent();
                InitializeCustomComponents();
                InitializeEventHandlers();
                InitializeControllers();

                LogTimer.Start();
                Console.WriteLine("MainForm initialized successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"CRITICAL ERROR in MainForm constructor: {ex}");
                MessageBox.Show($"Failed to initialize application: {ex.Message}", "Initialization Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                throw;
            }
        }
        #endregion

        #region Initialization Methods
        private void InitializeCustomComponents()
        {
            // Initialize execution monitor timer
            _executionMonitorTimer = new System.Windows.Forms.Timer();
            _executionMonitorTimer.Interval = 500;
            _executionMonitorTimer.Tick += ExecutionMonitorTimer_Tick;

            // Set up tooltips
            InitializeToolTips();

            // Configure UI defaults
            ConfigureUIDefaults();
        }

        private void InitializeToolTips()
        {
            var toolTip = new ToolTip();
            toolTip.SetToolTip(ConnectButton, "Connect to 3D printer");
            toolTip.SetToolTip(DisconnectButton, "Disconnect from 3D printer");
            toolTip.SetToolTip(ReadConfigButton, "Load printer configuration");
            toolTip.SetToolTip(ExecuteGcodeButton, "Execute G-code file");
            toolTip.SetToolTip(StopExecutionButton, "Stop current execution");
            toolTip.SetToolTip(PauseResumeButton, "Pause/Resume execution");
            toolTip.SetToolTip(ClearLogsButton, "Clear all logs");
            toolTip.SetToolTip(HomeButton, "Home all axes");
        }

        private void ConfigureUIDefaults()
        {
            // Set button colors
            ConnectButton.BackColor = Color.LightGreen;
            DisconnectButton.BackColor = Color.LightCoral;
            ExecuteGcodeButton.BackColor = Color.LightBlue;
            StopExecutionButton.BackColor = Color.LightSalmon;
            PauseResumeButton.BackColor = Color.LightYellow;

            // Configure text boxes
            LogTextBox.Font = new Font("Consolas", 9);
            InterpreterTextBox.Font = new Font("Consolas", 9);

            // Set form properties
            this.Text = "LP Studio 2.0 - 3D Printer Controller";
            this.Icon = SystemIcons.Application;
        }

        private void InitializeEventHandlers()
        {
            // Connection and control
            ConnectButton.Click += ConnectButton_Click;
            DisconnectButton.Click += DisconnectButton_Click;
            ReadConfigButton.Click += ReadConfigButton_Click;
            ExecuteGcodeButton.Click += ExecuteGcodeButton_Click;
            StopExecutionButton.Click += StopExecutionButton_Click;
            PauseResumeButton.Click += PauseResumeButton_Click;
            ClearLogsButton.Click += ClearLogsButton_Click;

            // Manual control
            HomeButton.Click += HomeButton_Click;
            ForceMoveUpButton.Click += (s, e) => MoveManual("Y", 10);
            ForceMoveDownButton.Click += (s, e) => MoveManual("Y", -10);
            ForceMoveLeftButton.Click += (s, e) => MoveManual("X", -10);
            ForceMoveRightButton.Click += (s, e) => MoveManual("X", 10);
            MoveZUpButton.Click += (s, e) => MoveManual("Z", 10);
            MoveZDownButton.Click += (s, e) => MoveManual("Z", -10);

            // Logging and UI
            LogTimer.Tick += LogTimer_Tick;
            AutoScrollCheckBox.CheckedChanged += AutoScrollCheckBox_CheckedChanged;

            // Form events
            this.FormClosing += MainForm_FormClosing;
        }

        private void InitializeControllers()
        {
            try
            {
                Console.WriteLine("Initializing PrinterController...");
                _printerController = new PrinterController();
                Console.WriteLine("PrinterController initialized successfully");

                Console.WriteLine("Initializing GCodeInterpreter...");
                _interpreter = new GCodeInterpreter();
                Console.WriteLine("GCodeInterpreter initialized successfully");

                UpdateStatus("Initialized and ready");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR initializing controllers: {ex}");
                UpdateStatus($"Initialization failed: {ex.Message}");
                throw;
            }
        }
        #endregion

        #region Event Handlers
        private async void ConnectButton_Click(object sender, EventArgs e)
        {
            await ExecuteWithButtonState(ConnectButton, "Connecting...", async () =>
            {
                Console.WriteLine("Attempting to connect...");
                bool isConnected = await Task.Run(() => _printerController.Connect());

                if (isConnected)
                {
                    Console.WriteLine("Connected successfully!");
                    UpdateStatus("Connected to printer");
                    UpdateConnectionState(true);
                }
                else
                {
                    string error = _printerController.GetLastError();
                    Console.WriteLine($"Failed to connect: {error}");
                    UpdateStatus($"Connection failed: {error}");
                    MessageBox.Show($"Connection failed: {error}", "Connection Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            });
        }

        private void DisconnectButton_Click(object sender, EventArgs e)
        {
            try
            {
                Console.WriteLine("Disconnecting...");
                _printerController.Disconnect();
                Console.WriteLine("Disconnected successfully");
                UpdateStatus("Disconnected");
                UpdateConnectionState(false);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in DisconnectButton_Click: {ex}");
                UpdateStatus($"Disconnect error: {ex.Message}");
            }
        }

        private void ReadConfigButton_Click(object sender, EventArgs e)
        {
            try
            {
                using (var openFileDialog = new OpenFileDialog())
                {
                    openFileDialog.Filter = "Config files (*.cfg)|*.cfg|All files (*.*)|*.*";
                    openFileDialog.Title = "Select printer configuration file";

                    if (openFileDialog.ShowDialog() == DialogResult.OK)
                    {
                        bool success = _interpreter.ReadConfig(openFileDialog.FileName);

                        if (success)
                        {
                            Console.WriteLine("Configuration loaded successfully");
                            UpdateStatus("Configuration loaded");
                            MessageBox.Show("Configuration loaded successfully!", "Success",
                                MessageBoxButtons.OK, MessageBoxIcon.Information);
                        }
                        else
                        {
                            string error = _interpreter.GetLastError();
                            Console.WriteLine($"Failed to load config: {error}");
                            UpdateStatus($"Config load failed: {error}");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Config error: {ex.Message}");
                UpdateStatus($"Config error: {ex.Message}");
            }
        }

        private async void ExecuteGcodeButton_Click(object sender, EventArgs e)
        {
            await ExecuteWithButtonState(ExecuteGcodeButton, "Executing...", async () =>
            {
            Console.WriteLine("=== ExecuteGcodeButton_Click START ===");

            string filename = "G-code.txt";
            string fullPath = Path.GetFullPath(filename);

            Console.WriteLine($"C#: Full path: {fullPath}");
            Console.WriteLine($"C#: File exists: {File.Exists(fullPath)}");

            if (!File.Exists(fullPath))
            {
                MessageBox.Show($"File '{fullPath}' not found!\nPlease create a G-code.txt file in the application directory.",
                    "File Not Found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var printerHandle = _printerController.GetPrinterHandle();
            if (printerHandle.VirtualTable == IntPtr.Zero)
            {
                MessageBox.Show("Printer is not connected!\nPlease connect to the printer first.",
                    "Printer Not Connected", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            // Check interpreter status
            var status = _interpreter.GetStatus();
            Console.WriteLine($"C#: Current interpreter status: {status}");

            if (status == GCodeInterpreter.Status.RUNNING ||
                status == GCodeInterpreter.Status.PAUSED ||
                status == GCodeInterpreter.Status.CHECKING_CODE)
            {
                MessageBox.Show("Interpreter is busy.\nPlease wait for current execution to complete.",
                    "Interpreter Busy", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            // Clear errors if any
            if (status == GCodeInterpreter.Status.ERROR)
            {
                _interpreter.ClearErrors();
            }

            Console.WriteLine("C#: Calling Interpreter.ExecuteFile...");
            bool success = await Task.Run(() => _interpreter.ExecuteFile(fullPath, printerHandle));

            Console.WriteLine($"C#: ExecuteFile returned: {success}");

            if (success)
            {
                Console.WriteLine("G-code execution started successfully");
                UpdateStatus("Executing G-code...");
                StartExecutionMonitoring();
            }
            else
            {
                string error = _interpreter.GetLastError();
                Console.WriteLine($"C#: ExecuteFile failed: {error}");
                UpdateStatus($"Execution failed: {error}");
                MessageBox.Show($"Failed to execute G-code:\n{error}", "Execution Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            Console.WriteLine("=== ExecuteGcodeButton_Click END ===");

            });
        }

        private void StopExecutionButton_Click(object sender, EventArgs e)
        {
            try
            {
                _interpreter.Stop();
                UpdateStatus("Execution stopped");
                _executionMonitorTimer.Stop();
                Console.WriteLine("Execution stopped by user");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error stopping execution: {ex.Message}");
                UpdateStatus($"Stop error: {ex.Message}");
            }
        }

        private void PauseResumeButton_Click(object sender, EventArgs e)
        {
            try
            {
                var status = _interpreter.GetStatus();
                if (status == GCodeInterpreter.Status.RUNNING)
                {
                    _interpreter.Pause();
                    PauseResumeButton.Text = "Resume";
                    PauseResumeButton.BackColor = Color.LightGreen;
                    UpdateStatus("Execution paused");
                }
                else if (status == GCodeInterpreter.Status.PAUSED)
                {
                    _interpreter.Resume();
                    PauseResumeButton.Text = "Pause";
                    PauseResumeButton.BackColor = Color.LightYellow;
                    UpdateStatus("Execution resumed");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in pause/resume: {ex.Message}");
                UpdateStatus($"Pause/Resume error: {ex.Message}");
            }
        }

        private void ClearLogsButton_Click(object sender, EventArgs e)
        {
            try
            {
                LogTextBox.Clear();
                InterpreterTextBox.Clear();
                _interpreter.ClearLog();
                _printerController.ClearLog();
                _lastLogCount = 0;
                _lastInterpreterLogCount = 0;
                UpdateStatus("Logs cleared");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error clearing logs: {ex.Message}");
            }
        }

        private void HomeButton_Click(object sender, EventArgs e)
        {
            ExecuteManualCommand("G28", "Homing all axes");
        }

        private void MoveManual(string axis, double distance)
        {
            string command = $"G91 G0 {axis}{distance} F1000";
            ExecuteManualCommand(command, $"Moving {axis} by {distance}mm");
        }

        private async void ExecuteManualCommand(string command, string description)
        {
            try
            {
                var printerHandle = _printerController.GetPrinterHandle();
                if (printerHandle.VirtualTable == IntPtr.Zero)
                {
                    MessageBox.Show("Printer is not connected!", "Printer Not Connected",
                        MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                Console.WriteLine($"Manual command: {command} - {description}");
                bool success = await Task.Run(() => _interpreter.ExecuteLine(command, printerHandle));

                if (success)
                {
                    UpdateStatus(description);
                }
                else
                {
                    string error = _interpreter.GetLastError();
                    UpdateStatus($"Manual move failed: {error}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in manual command: {ex.Message}");
                UpdateStatus($"Manual move error: {ex.Message}");
            }
        }

        private void AutoScrollCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            _autoScrollEnabled = AutoScrollCheckBox.Checked;
        }
        private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                Console.WriteLine("MainForm is closing, disposing resources...");
                LogTimer.Stop();
                _executionMonitorTimer.Stop();
                _interpreter?.Dispose();
                _printerController?.Dispose();
                Console.WriteLine("Resources disposed successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR during disposal: {ex}");
            }
        }
        #endregion

        #region UI Update Methods
        private void LogTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                UpdateLogDisplay();
                UpdateInterpreterLogDisplay();
                UpdateStatusDisplay();
                UpdateManualControlState();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"ERROR in LogTimer_Tick: {ex}");
            }
        }

        private void UpdateLogDisplay()
        {
            try
            {
                int currentLogCount = _printerController.GetLogCount();
                if (currentLogCount > _lastLogCount)
                {
                    for (int i = _lastLogCount; i < currentLogCount; i++)
                    {
                        string logEntry = _printerController.GetLogEntry(i);
                        if (!string.IsNullOrEmpty(logEntry))
                        {
                            LogTextBox.AppendText($"[PRINTER] {logEntry}{Environment.NewLine}");
                        }
                    }
                    _lastLogCount = currentLogCount;
                    ScrollToEnd(LogTextBox);
                }
            }
            catch (Exception ex)
            {
                LogTextBox.AppendText($"[UI ERROR] Failed to update log: {ex.Message}{Environment.NewLine}");
            }
        }

        private void UpdateInterpreterLogDisplay()
        {
            try
            {
                int currentLogCount = _interpreter.GetLogCount();
                if (currentLogCount > _lastInterpreterLogCount)
                {
                    for (int i = _lastInterpreterLogCount; i < currentLogCount; i++)
                    {
                        string logEntry = _interpreter.GetLog(i);
                        if (!string.IsNullOrEmpty(logEntry))
                        {
                            InterpreterTextBox.AppendText($"[INTERPRETER] {logEntry}{Environment.NewLine}");
                        }
                    }
                    _lastInterpreterLogCount = currentLogCount;
                    ScrollToEnd(InterpreterTextBox);
                }
            }
            catch (Exception ex)
            {
                InterpreterTextBox.AppendText($"[UI ERROR] Failed to update interpreter log: {ex.Message}{Environment.NewLine}");
            }
        }

        private void ScrollToEnd(TextBox textBox)
        {
            if (_autoScrollEnabled)
            {
                textBox.SelectionStart = textBox.Text.Length;
                textBox.ScrollToCaret();
            }
        }

        private void UpdateStatusDisplay()
        {
            try
            {
                var status = _interpreter.GetStatus();
                double progress = _interpreter.GetProgress();
                int errorCount = _interpreter.GetErrorCount();

                StatusLabel.Text = $"Status: {GetStatusString(status)} | Progress: {progress:F1}% | Errors: {errorCount}";

                // Update progress bar
                ProgressBar.Value = (int)progress;

                // Update status color
                UpdateStatusColor(status);
            }
            catch (Exception ex)
            {
                StatusLabel.Text = $"Status: Error - {ex.Message}";
            }
        }
        private string GetStatusString(GCodeInterpreter.Status status)
        {
            return status switch
            {
                GCodeInterpreter.Status.IDLE => "Idle",
                GCodeInterpreter.Status.CHECKING_CODE => "Checking Code",
                GCodeInterpreter.Status.RUNNING => "Running",
                GCodeInterpreter.Status.PAUSED => "Paused",
                GCodeInterpreter.Status.COMPLETED => "Completed",
                GCodeInterpreter.Status.ERROR => "Error",
                _ => "Unknown"
            };
        }

        private void UpdateStatusColor(GCodeInterpreter.Status status)
        {
            Color color = status switch
            {
                GCodeInterpreter.Status.IDLE => Color.LightGray,
                GCodeInterpreter.Status.CHECKING_CODE => Color.LightBlue,
                GCodeInterpreter.Status.RUNNING => Color.LightGreen,
                GCodeInterpreter.Status.PAUSED => Color.LightYellow,
                GCodeInterpreter.Status.COMPLETED => Color.Green,
                GCodeInterpreter.Status.ERROR => Color.LightCoral,
                _ => Color.White
            };

            StatusLabel.BackColor = color;
        }

        private void UpdateConnectionState(bool isConnected)
        {
            ConnectButton.Enabled = !isConnected;
            DisconnectButton.Enabled = isConnected;
            ExecuteGcodeButton.Enabled = isConnected;
            StopExecutionButton.Enabled = isConnected;
            PauseResumeButton.Enabled = isConnected;

            ConnectionStatusLabel.Text = isConnected ? "Connected" : "Disconnected";
            ConnectionStatusLabel.BackColor = isConnected ? Color.LightGreen : Color.LightCoral;
        }

        private void UpdateManualControlState()
        {
            var status = _interpreter.GetStatus();
            bool canManualControl = status == GCodeInterpreter.Status.IDLE ||
                                  status == GCodeInterpreter.Status.COMPLETED;

            HomeButton.Enabled = canManualControl;
            ForceMoveUpButton.Enabled = canManualControl;
            ForceMoveDownButton.Enabled = canManualControl;
            ForceMoveLeftButton.Enabled = canManualControl;
            ForceMoveRightButton.Enabled = canManualControl;
            MoveZUpButton.Enabled = canManualControl;
            MoveZDownButton.Enabled = canManualControl;
        }

        private void UpdateStatus(string message)
        {
            StatusLabel.Text = message;
            Console.WriteLine($"Status: {message}");
        }
        #endregion

        #region Execution Monitoring
        private void StartExecutionMonitoring()
        {
            _executionMonitorTimer.Start();
        }

        private void ExecutionMonitorTimer_Tick(object sender, EventArgs e)
        {
            var status = _interpreter.GetStatus();
            var progress = _interpreter.GetProgress();

            ProgressBar.Value = (int)progress;

            if (status == GCodeInterpreter.Status.COMPLETED ||
                status == GCodeInterpreter.Status.ERROR ||
                status == GCodeInterpreter.Status.IDLE)
            {
                _executionMonitorTimer.Stop();
                PauseResumeButton.Text = "Pause";
                PauseResumeButton.BackColor = Color.LightYellow;

                if (status == GCodeInterpreter.Status.COMPLETED)
                {
                    UpdateStatus("Execution completed successfully");
                    MessageBox.Show("G-code execution completed successfully!", "Completed",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else if (status == GCodeInterpreter.Status.ERROR)
                {
                    string error = _interpreter.GetLastError();
                    UpdateStatus($"Execution failed: {error}");
                }
            }
        }
        #endregion

        #region Helper Methods
        private async Task ExecuteWithButtonState(Button button, string busyText, Func<Task> action)
        {
            var originalText = button.Text;
            var originalColor = button.BackColor;

            try
            {
                button.Enabled = false;
                button.Text = busyText;
                button.BackColor = Color.LightGray;

                await action();
            }
            finally
            {
                button.Enabled = true;
                button.Text = originalText;
                button.BackColor = originalColor;
            }
        }
        #endregion
    }
}