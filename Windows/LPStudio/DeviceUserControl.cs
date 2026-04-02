using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Printing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace LPStudio
{
    public partial class DeviceUserControl : UserControl
    {
        private PrinterController printerController;
        private InterpreterController interpreterController;
        private BatteryLabel batteryLabel;

        private int lastDriverLogCount = 0;
        private int lastLogCount = 0;
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
                interpreterController = new InterpreterController(printerController.GetPrinterHandle());
                InitializeComponent();

                textBoxConsole.MaxLength = 0;

                batteryLabel = new BatteryLabel();
                batteryLabel.Location = this.labelBattery.Location;
                batteryLabel.Size = this.labelBattery.Size;
                batteryLabel.Name = this.labelBattery.Name;
                batteryLabel.TabIndex = this.labelBattery.TabIndex;
                batteryLabel.TextAlign = this.labelBattery.TextAlign;
                batteryLabel.Font = this.labelBattery.Font;

                int index = this.panelConnection.Controls.IndexOf(this.labelBattery);
                if (index >= 0)
                {
                    this.panelConnection.Controls.Remove(this.labelBattery);
                    this.panelConnection.Controls.Add(batteryLabel);
                    this.panelConnection.Controls.SetChildIndex(batteryLabel, index);
                }

                this.labelBattery = batteryLabel;
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

            void resetConnectUi()
            {
                connectButton.BackColor = Color.FromArgb(29, 175, 30);
                connectButton.Text = "Connect";
                connectButton.Enabled = true;
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

            if (!printerController.IsPrinterConnect())
            {
                connectButton.BackColor = Color.FromArgb(227, 235, 12);
                connectButton.Text = "Connecting...";

                try
                {
                    bool connected = await Task.Run(() => printerController.ConnectAuto(10000, true));
                    if (!connected)
                    {
                        MessageBox.Show("No Lego hub found!");
                        resetConnectUi();
                        return;
                    }

                    string address = printerController.GetConnectedAddress();
                    if (string.IsNullOrEmpty(address))
                    {
                        throw new Exception("Connected, but address is empty");
                    }

                    connectButton.Text = "Checking hub mode...";
                    var mode = await Task.Run(() => printerController.DetectHubMode(address));
                    
                    if (mode != PrinterController.HubMode.PrinterRuntime)
                    {
                        var needFirmware = (mode == PrinterController.HubMode.LegoOfficial || mode == PrinterController.HubMode.Bootloader);
                        if (!needFirmware)
                        {
                            bool runtimeReady = await Task.Run(() => printerController.ProbeRuntime(address, 2000));
                            if (!runtimeReady)
                            {
                                MessageBox.Show("Hub is in an unknown state. Please restart it and try again");
                                bool disconnect = await Task.Run(() => printerController.Disconnect());
                                resetConnectUi();
                                return;
                            }

                        }
                        else
                        {
                            var result = MessageBox.Show(
                                "This hub needs to be flashed with firmware to work with the printer\n\n" + 
                                "Do you want to install the firmware now?",
                                "Firmware required",
                                MessageBoxButtons.YesNo,
                                MessageBoxIcon.Question);

                            if (result != DialogResult.Yes)
                            {
                                await Task.Run(() => printerController.Disconnect());
                                resetConnectUi();
                                return;
                            }

                            connectButton.Text = "Installing firmware...";
                            connectButton.BackColor = Color.FromArgb(255, 165, 0);

                            string firmwarePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "printer-firmware-v3.6.1/firmware-base.bin");

                            bool firmwareOk = await Task.Run(() => printerController.FlashFirmware(firmwarePath, address));
                            if (!firmwareOk)
                            {
                                MessageBox.Show("Firmware installation failed. Please check the console for details", "Error");

                                bool disconnectOk = await Task.Run(() => printerController.Disconnect());
                                resetConnectUi();
                                return;
                            } 

                            connectButton.Text = "Restarting...";
                            await Task.Delay(10000);

                            bool disconnect = await Task.Run(() => printerController.Disconnect());
                            
                            await Task.Delay(5000);

                            connected = await Task.Run(() => printerController.ConnectAuto(10000, false));
                            if (!connected)
                            {
                                MessageBox.Show("Hub did not reappear after firmware flash. Please turn it off and on manually");
                                resetConnectUi();
                                return;
                            }
                            address = printerController.GetConnectedAddress();
                        }
                    }

                    connectButton.Text = "Checking runtime...";
                    bool runtimePresent = await Task.Run(() => printerController.ProbeRuntime(address, 2000));

                    if (!runtimePresent)
                    {
                        var result = MessageBox.Show(
                            "The printer control program is not installed on the hub\n\n" + 
                            "Do you want to upload it now?",
                            "Runtime required",
                            MessageBoxButtons.YesNo,
                            MessageBoxIcon.Question);

                        if (result != DialogResult.Yes)
                        {
                            await Task.Run(() => printerController.Disconnect());
                            resetConnectUi();
                            return;
                        }

                        connectButton.Text = "Uploading runtime...";
                        connectButton.BackColor = Color.FromArgb(255, 165, 0);

                        string scriptPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "legoWirelessProtocol-firmwareScript.py");
                        bool uploadOk = await Task.Run(() => printerController.UploadProgram(scriptPath, address));
                        if (!uploadOk)
                        {
                            MessageBox.Show("Failed to upload runtime program. Check console.");

                            await Task.Run(() => printerController.Disconnect());
                            resetConnectUi();
                            return;
                        }

                        bool started = await Task.Run(() => printerController.StartUserProgram());
                        if (!started)
                        {
                            MessageBox.Show("Runtime program uploaded but failed to start");
                        }                        
                    }

                    await Task.Delay(1000);

                    runtimePresent = await Task.Run(() => printerController.ProbeRuntime(address, 2000));
                    if (!runtimePresent)
                    {
                        MessageBox.Show("Runtime did not become ready after upload.");

                        await Task.Run(() => printerController.Disconnect());
                        resetConnectUi();
                        return;
                    }

                    connectButton.Text = "Connecting to runtime...";
                    bool runtimeConnected = await Task.Run(() => printerController.ConnectRuntime(address));
                    if (!runtimeConnected)
                    {
                        MessageBox.Show("Failed to establish runtime communication with the hub");
                        await Task.Run(() => printerController.Disconnect());
                        resetConnectUi();
                        return;
                    }
                    resetConnectUi();
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.ToString());

                    await Task.Run(() => printerController.Disconnect());
                    resetConnectUi();
                    return;
                }
            }
            else
            {
                connectButton.BackColor = Color.FromArgb(227, 235, 12);
                connectButton.Text = "Disconnecting...";

                try
                {
                    bool disconnected = await Task.Run(() => printerController.Disconnect());
                    if (!disconnected)
                    {
                        MessageBox.Show("Disconnect failed");
                    }

                    resetConnectUi();
                    return;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error: {ex.ToString()}");
                    resetConnectUi();
                    return;
                }
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
                        stringBuilder.AppendLine($"[Driver] {logEntry}");
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
            lastLogCount = 0;
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
        }

        private void moveYUpButton_Click(object sender, EventArgs e)
        {

        }

        private void moveYDownButton_Click(object sender, EventArgs e)
        {

        }
    }
    public class BatteryLabel : Label
    {
        private PrinterController printer;
        private Timer updateTimer;
        private ToolTip batteryToolTip;

        // battery
        private int batteryLevel = 0;
        private DateTime lastUpdateTime = DateTime.MinValue;
        bool isConnected = false;

        private int batteryWidth = 40;
        private int batteryHeight = 20;
        private int capWidth = 5;
        private int capHeight = 10;
        private int padding = 2;

        private Color borderColor = Color.White;
        private Color fillColor = Color.Gray;
        private Color textColor = Color.White;
        private Color disconnectedColor = Color.LightGray;

        public BatteryLabel()
        {
            this.AutoSize = false;
            this.Size = new Size(100, 30);
            this.TextAlign = ContentAlignment.MiddleCenter;

            batteryToolTip = new ToolTip();
            batteryToolTip.AutoPopDelay = 5000;
            batteryToolTip.InitialDelay = 1000;
            batteryToolTip.ReshowDelay = 500;
            batteryToolTip.ShowAlways = true;
            batteryToolTip.SetToolTip(this, "The battery is not connected");

            updateTimer = new Timer();
            updateTimer.Interval = 5000;
            updateTimer.Tick += (s, e) => UpdateBatteryInfo();
            updateTimer.Start();
        }
        public void RefreshBatteryState()
        {
            if (printer != null && isConnected)
            {
                UpdateBatteryInfo();
            }
            else
            {
                UpdateConnectionState();
                this.Invalidate();
            }
        }
        public void SetPrinterController(PrinterController currentPrinter)
        {
            printer = currentPrinter;

            if (printer != null)
            {
                UpdateConnectionState();
                updateTimer.Start();
                UpdateBatteryInfo();
            }
            else
            {
                updateTimer.Stop();
                isConnected = false;
                batteryLevel = 0;
                this.Invalidate();
            }
        }
        private void UpdateConnectionState()
        {
            if (printer == null)
            {
                isConnected = false;
                return;
            }

            try
            {
                isConnected = printer.IsPrinterConnect();
            }
            catch
            {
                isConnected = false;
            }
        }
        private void UpdateBatteryInfo()
        {
            if (printer == null)
            {
                batteryLevel = 0;
                isConnected = false;
                this.Invalidate();
                return;
            }

            try
            {
                UpdateConnectionState();

                if (!isConnected)
                {
                    batteryLevel = 0;
                    this.Invalidate();
                    return;
                }
                byte newLevel = 0;
                try
                {
                    // Get the level
                    newLevel = printer.GetBatteryLevel();

                    System.Threading.Thread.Sleep(50);
                    // Checking the correctness
                    if (newLevel > 100)
                    {
                        newLevel = 100;
                    }
                }
                catch
                {
                }

                if (newLevel != batteryLevel)
                {
                    batteryLevel = newLevel;
                    lastUpdateTime = DateTime.Now;

                    UpdateColors();
                    this.Invalidate();
                    UpdateToolTip();
                }
            }
            catch (Exception ex)
            {
                batteryLevel = 0;
                isConnected = false;
                this.Invalidate();
            }
        }
        private void UpdateColors()
        {
            if (!isConnected)
            {
                fillColor = disconnectedColor;
                textColor = Color.Gray;
            }
            else if (batteryLevel < 16)
            {
                fillColor = Color.Red;
                textColor = Color.White;
            }
            else if (batteryLevel < 31)
            {
                fillColor = Color.Orange;
                textColor = Color.White;
            }
            else if (batteryLevel < 51)
            {
                fillColor = Color.Yellow;
                textColor = Color.White;
            }
            else
            {
                fillColor = Color.LimeGreen;
                textColor = Color.White;
            }
        }
        private void UpdateToolTip()
        {
            string tooltipText = $"{batteryLevel}%\n";
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;

            // Calculate the battery position (left)
            int batteryX = padding;
            int batteryY = (this.Height - batteryHeight) / 2;

            // Draw the battery
            DrawBattery(g, batteryX, batteryY);

            // Draw text (to the right of the battery)
            DrawText(g, batteryX + batteryWidth + capWidth + 5);
        }
        private void DrawBattery(Graphics g, int x, int y)
        {
            // Main battery body
            Rectangle batteryRect = new Rectangle(x, y, batteryWidth, batteryHeight);

            using (Pen borderPen = new Pen(borderColor, 1))
            {
                g.DrawRectangle(borderPen, batteryRect);
            }

            // Cap (right)
            int capY = y + (batteryHeight - capHeight) / 2;
            Rectangle capRect = new Rectangle(
                x + batteryWidth,
                capY,
                capWidth,
                capHeight
            );

            g.FillRectangle(new SolidBrush(borderColor), capRect);

            // Checking the connection and charge
            if (isConnected && batteryLevel > 0)
            {
                // Calculate the padding width taking into account the borders
                int innerWidth = batteryWidth - 2; // -2 for left and right borders
                int fillWidth = (int)(innerWidth * batteryLevel / 100.0);

                // Limit fillWidth so it doesn't go beyond the boundaries
                fillWidth = Math.Max(1, Math.Min(fillWidth, innerWidth));

                Rectangle fillRect = new Rectangle(
                    x + 1, // +1 for the inner border
                    y + 1, // +1 for the inner border
                    fillWidth,
                    batteryHeight - 2 // -2 for upper and lower bounds
                );

                g.FillRectangle(new SolidBrush(fillColor), fillRect);

                // Light gradient for beauty
                using (LinearGradientBrush brush = new LinearGradientBrush(
                    fillRect,
                    Color.FromArgb(150, Color.White),
                    Color.Transparent,
                    90f))
                {
                    g.FillRectangle(brush,
                        fillRect.X, fillRect.Y,
                        fillRect.Width, fillRect.Height / 3);
                }
            }
            else if (!isConnected)
            {
                // Gray background for disabled state
                Rectangle emptyRect = new Rectangle(
                    x + 1,
                    y + 1,
                    batteryWidth - 2,
                    batteryHeight - 2
                );

                g.FillRectangle(new SolidBrush(disconnectedColor), emptyRect);

                // Cross
                using (Pen crossPen = new Pen(Color.Gray, 1))
                {
                    int crossSize = Math.Min(emptyRect.Width, emptyRect.Height) / 2;
                    int crossX = emptyRect.X + (emptyRect.Width - crossSize) / 2;
                    int crossY = emptyRect.Y + (emptyRect.Height - crossSize) / 2;

                    g.DrawLine(crossPen, crossX, crossY, crossX + crossSize, crossY + crossSize);
                    g.DrawLine(crossPen, crossX + crossSize, crossY, crossX, crossY + crossSize);
                }
            }
        }

        private void DrawText(Graphics g, int textX)
        {
            string text = isConnected ? $"{batteryLevel}%" : "---";

            // Use the font with the correct settings
            using (Font font = new Font("Arial", 9, FontStyle.Bold))
            {
                // Measure the text for correct positioning
                SizeF textSize = g.MeasureString(text, font);
                int textY = (this.Height - (int)textSize.Height) / 2;

                // Draw text with a slight offset for better readability
                RectangleF textRect = new RectangleF(textX, textY, textSize.Width, textSize.Height);

                // Draw a background for the text (optional, for better readability)
                g.FillRectangle(new SolidBrush(Color.FromArgb(100, 0, 0, 0)), textRect);

                // Shadow for better readability
                g.DrawString(text, font, Brushes.Black, textX + 1, textY + 1);

                // Main text
                using (SolidBrush textBrush = new SolidBrush(textColor))
                {
                    g.DrawString(text, font, textBrush, textX, textY);
                }
            }
        }

        protected override void OnClick(EventArgs e)
        {
            base.OnClick(e);
            UpdateBatteryInfo();
        }

        public void StopMonitoring()
        {
            updateTimer?.Stop();
        }
    }
}
