using System;
using System.Drawing;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace LPStudio
{
    public partial class DeviceUserControl : UserControl
    {
        private PrinterController printerController;

        private int lastDriverLogCount = 0;
        private readonly object updateLock = new object();
        private bool isConsoleUpdating = false;

        [Flags]
        public enum LogCategory : uint
        {
            None = 0,
            Error = 1 << 0,
            Warning = 1 << 1,
            Info = 1 << 2,
            Debug = 1 << 3,
            Motor = 1 << 4,
            Encoder = 1 << 5,
            Bluetooth = 1 << 6,
            Profile = 1 << 7,
            Performance = 1 << 8,
            Command = 1 << 9,
            All = 0xFFFFFFFF,
            Default = Error | Warning | Info | Motor | Encoder,
        }

        private LogCategory enabledCategories = LogCategory.Default;
        public DeviceUserControl()
        {
            try
            {
                printerController = new PrinterController();
                InitializeComponent();

                textBoxConsole.MaxLength = 0;

                logTimer.Interval = 100;
                logTimer.Start();
            }
            catch (Exception)
            {
                throw;
            }
        }
        private async void connectButton_Click(object sender, EventArgs e)
        {
            connectButton.Enabled = false;

            void ResetConnectUi()
            {
                connectButton.BackColor = Color.FromArgb(29, 175, 30);
                connectButton.Text = "Connect";
                connectButton.Enabled = true;
            }

            void SetConnectUi(string text, Color color)
            {
                connectButton.BackColor = color;
                connectButton.Text = text;
            }

            async Task DisconnectSafe()
            {
                try
                {
                    await Task.Run(() => printerController.Disconnect());
                }
                catch
                {
                    // Best effort cleanup
                }
            }

            async Task<bool> WaitForHubModeAsync(PrinterController.HubMode expectedMode, int totalWaitMs = 90000, int connectTimeoutMs = 10000)
            {
                const int stepMs = 5000;

                for (int elapsed = 0; elapsed < totalWaitMs; elapsed += stepMs)
                {
                    await Task.Delay(stepMs);

                    bool connected = await Task.Run(() =>
                    printerController.ConnectAuto(connectTimeoutMs, false));
                    if (!connected)
                    {
                        continue;
                    }

                    string address = printerController.GetConnectedAddress();
                    if (string.IsNullOrWhiteSpace(address))
                    {
                        await DisconnectSafe();
                        continue;
                    }

                    var mode = await Task.Run(() =>
                    printerController.DetectHubMode(address));

                    Console.WriteLine($"[C#] WaitForHubModeAsync: mode={mode}, expected={expectedMode}");

                    if (mode == expectedMode)
                    {
                        return true;
                    }

                    if (mode == PrinterController.HubMode.PybricksRuntime)
                    {
                        continue;
                    }

                    await DisconnectSafe();
                }

                return false;
            }

            try
            {
                if (printerController.IsPrinterConnect())
                {
                    SetConnectUi("Disconnecting...", Color.FromArgb(227, 235, 12));

                    bool disconnected = await Task.Run(() => printerController.Disconnect());
                    if (!disconnected)
                    {
                        MessageBox.Show("Disconnect failed");
                    }

                    ResetConnectUi();
                    return;
                }

                SetConnectUi("Connecting", Color.FromArgb(227, 235, 12));

                bool connected = await Task.Run(() => printerController.ConnectAuto(10000, true));
            
                if (!connected)
                {
                    MessageBox.Show("No hub found!");
                    ResetConnectUi();
                    return;
                }

                string address = printerController.GetConnectedAddress();
                if (string.IsNullOrWhiteSpace (address))
                {
                    throw new Exception("Connected, but address is empty");
                }

                SetConnectUi("Checking hub mode...", Color.FromArgb(227, 235, 12));
                var mode = await Task.Run(() => printerController.DetectHubMode(address));
            
                if (mode == PrinterController.HubMode.Bootloader || mode == PrinterController.HubMode.LegoOfficial)
                {
                    var result = MessageBox.Show("This hub needs to be flashed with firmware to work with the printer\n\nDo you want to install the firmware now?", "Firmware required", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                    if (result != DialogResult.Yes)
                    {
                        await DisconnectSafe();
                        ResetConnectUi();
                        return;                    
                    }

                    SetConnectUi("Installing firmware...", Color.FromArgb(225, 165, 0));

                    string firmwarePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "firmware.blob");

                    bool firmwareResult = await Task.Run(() => printerController.FlashFirmware(firmwarePath, address));
                    if (!firmwareResult)
                    {
                        MessageBox.Show("Firmware installation failed. Please check the console for details", "Error");
                        await DisconnectSafe();
                        ResetConnectUi();
                        return;
                    }

                    SetConnectUi("Waiting for hub reboot...", Color.FromArgb(255, 165, 0));

                    await Task.Delay(25000);

                    bool runtimeUp = await WaitForHubModeAsync(PrinterController.HubMode.PybricksRuntime, 120000, 10000);
                    if (!runtimeUp)
                    {
                        MessageBox.Show("Hub did not come back in Pybricks runtime mode after flashing.\n\nTry power-cycling it once", "Runtime not ready", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        
                        await DisconnectSafe();
                        ResetConnectUi();
                        return;
                    }

                    address = printerController.GetConnectedAddress();
                    if (string.IsNullOrWhiteSpace(address))
                    {
                        throw new Exception("Reconnected, but address is empty");
                    }
                }
                else if (mode != PrinterController.HubMode.PybricksRuntime)
                {
                    MessageBox.Show("Hub is an unknown state. Please restart it and try again.");
                    await DisconnectSafe();
                    ResetConnectUi();
                    return;
                }

                SetConnectUi("Checking runtime program...", Color.FromArgb(227, 235, 12));

                bool applicationReady = false;

                if (!applicationReady)
                {
                    var result = MessageBox.Show("The printer control program is not installed on the hub\n\nDo you want to upload it now?",
                        "Runtime required", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                    if (result != DialogResult.Yes)
                    {
                        await DisconnectSafe();
                        ResetConnectUi();
                        return;
                    }

                    SetConnectUi("Uploading runtime...", Color.FromArgb(255, 165, 0));

                    string scriptData = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "main.mpy");

                    bool uploadResult = await Task.Run(() => printerController.UploadProgram(scriptData, address));
                    if (!uploadResult)
                    {
                        MessageBox.Show("Failed to upload runtime program. Check console");
                        await DisconnectSafe();
                        ResetConnectUi();
                        return;
                    }
                }

                SetConnectUi("Connecting to runtime...", Color.FromArgb(227, 235, 12));

                await Task.Delay(1000);

                bool runtimeConnected = await Task.Run(() => printerController.ConnectRuntime(address));
                if (!runtimeConnected)
                {
                    MessageBox.Show("Failed to establish runtime communication with the hub");
                    await DisconnectSafe();
                    ResetConnectUi();
                    return;
                }

                connectButton.BackColor = Color.FromArgb(234, 84, 85);
                connectButton.Text = "Disconnect";
                connectButton.Enabled = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex);
                await DisconnectSafe();
                ResetConnectUi();
            }
        }
        private void logTimer_Tick(object sender, EventArgs e)
        {
            UpdateConsoleDisplay();
        }
        private void UpdateConsoleDisplay()
        {
            if (isConsoleUpdating) return;

            lock(updateLock)
            {
                if (isConsoleUpdating) return;
                isConsoleUpdating = true;
            }

            try
            {
                UpdateConsoleDisplayInternal();
            }
            finally
            {
                lock(updateLock)
                {
                    isConsoleUpdating = false;
                }
            }
        }

        // In the form/controller class, create a global counter for the number of records read
        private long totalProcessedLogs = 0;

        private void UpdateConsoleDisplayInternal()
        {
            // Get the total number of records that are currently in the C++ buffer
            int currentAvailable = printerController.GetLogCount();

            // If the buffer in C++ was cleared (ClearLog)
            if (currentAvailable == 0 && lastDriverLogCount > 0)
            {
                ClearTextBoxSafe();
                totalProcessedLogs = 0;
                lastDriverLogCount = 0;
                return;
            }

            // We need to figure out how many NEW records have appeared since the last time.
            // But since the buffer is circular, index 0 in C++ always points to the oldest available record.

            // The simplest and most reliable method for a TextBox:
            // Simply grab everything newer than our last read position.
            if (currentAvailable > 0)
            {
                // If we missed too many (the buffer in C++ rotated faster than the timer fired)
                // then we just take the last available records.
                int logsToRead = currentAvailable;

                // The logic here is: if we've already read the logs, we subtract what we've already seen.
                // But remember that GetLogEntry(0) is always the "oldest available."
                // Therefore, it's easiest to read the tail:
                AppendDriverLogs(0, currentAvailable);

                // IMPORTANT: After this, you need to have a "Clear Read" method in C++
                // or switch to absolute indexes (Sequence Numbers).
                printerController.ClearLog(); // If possible, clear the C++ buffer after reading
            }
        }

        private void AppendDriverLogs(int startIndex, int count)
        {
            if (count < 1) return;

            StringBuilder stringBuilder = new StringBuilder(count * 100);

            for (int i = 0; i < count; i++)
            {
                try
                {
                    string logEntry = printerController.GetLogEntry(startIndex + i);
                    if (!string.IsNullOrEmpty(logEntry))
                    {
                        stringBuilder.AppendLine(logEntry);
                    }
                }
                catch
                {
                    // Ignore errors getting one record
                }
            }

            if (stringBuilder.Length > 0)
            {
                AppendTextSafe(stringBuilder.ToString());
            }
        }        

        private void AppendTextSafe(string text)
        {
            if (textBoxConsole.InvokeRequired)
            {
                textBoxConsole.BeginInvoke((MethodInvoker)delegate
                {
                    // Fast insert with paused update
                    textBoxConsole.SuspendLayout();
                    textBoxConsole.AppendText(text);

                    // Autoscroll if enabled
                    if (AutoScrollEnabled)
                    {
                        textBoxConsole.SelectionStart = textBoxConsole.TextLength;
                        textBoxConsole.ScrollToCaret();
                    }

                    textBoxConsole.ResumeLayout();

                    // Limit the size for performance
                    LimitLogSize();
                });
            }
            else
            {
                textBoxConsole.SuspendLayout();
                textBoxConsole.AppendText(text);

                if (AutoScrollEnabled)
                {
                    textBoxConsole.SelectionStart = textBoxConsole.TextLength;
                    textBoxConsole.ScrollToCaret();
                }

                textBoxConsole.ResumeLayout();
                LimitLogSize();
            }
        }
        private void ClearTextBoxSafe()
        {
            if (textBoxConsole.InvokeRequired)
            {
                textBoxConsole.Invoke((MethodInvoker)delegate
                {
                    textBoxConsole.Clear();
                });
            }
            else
            {
                textBoxConsole.Clear();
            }
        }
        private void LimitLogSize()
        {
            const int MAX_CHARS = 100000; // Limit by characters, it's faster
            if (textBoxConsole.TextLength > MAX_CHARS)
            {
                // Remove the first 20% of the text
                textBoxConsole.Select(0, MAX_CHARS / 5);
                textBoxConsole.SelectedText = "";
                // The caret will remain at the end, AppendText will continue writing downwards
            }
        }
        public bool AutoScrollEnabled { get; set; } = true;
        public void ClearLogs()
        {
            printerController.ClearLog();
            ClearTextBoxSafe();
        }

        private void browseButton_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.InitialDirectory = "c:\\";
                openFileDialog.Filter = "g-code files (*.gcode)|*.gcode";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    string filePath = openFileDialog.FileName;
                    showCodeFile.Text = filePath;
                }
            }
        }
        private async void buttonExecuteCode_Click(object sender, EventArgs e)
        {
            buttonExecuteCode.Enabled = false;
            buttonExecuteCode.Text = "Executing";

            bool testResult = await Task.Run(() => printerController.PrinterExecuteCode(showCodeFile.Text));

            while (!testResult)
            {
                await Task.Delay(1000);
            }

            buttonExecuteCode.Enabled = true;
            buttonExecuteCode.Text = "Execute Code";
        }
    }
}
