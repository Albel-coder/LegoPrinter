using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WindowsForms
{
    public partial class MainForm : Form
    {
        private PrinterController printerController = new PrinterController();
        private GCodeInterpreter Interpreter = new GCodeInterpreter();
        private int LastLogCount = 0;
        private bool AutoScrollEnambled = true;

        public MainForm()
        {
            InitializeComponent();
            LogTimer.Start();
        }

        private async void ConnectButton_Click(object sender, EventArgs e)
        {
            ConnectButton.Enabled = false;
            ConnectButton.Text = "Connecting...";

            try
            {
                bool isConnected = await Task.Run(() => printerController.Connect());
            }
            catch(Exception ex)
            {
                MessageBox.Show($"Error with connect: {ex.Message}");
            }
            finally
            {
                ConnectButton.Enabled = true;
                ConnectButton.Text = "Connect";
            }
        }

        private void DisconnectButton_Click(object sender, EventArgs e)
        {
            printerController.Disconnect();
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            printerController?.Dispose();
            base.OnFormClosing(e);
        }

        private void Test_Click(object sender, EventArgs e)
        {
            var printerHandle = printerController.GetPrinterHandle();

            Interpreter.Test(printerHandle);
        }

        private void MotorTest_Click(object sender, EventArgs e)
        {
            printerController.Test();
        }
        private void LogTimer_Tick(object sender, EventArgs e)
        {
            UpdateLegoDisplay();
        }
        private void UpdateLegoDisplay()
        {
            try
            {
                int CurrentLogCount = printerController.GetLogCount();

                // Update only if we have new info
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

                    if (AutoScrollEnambled)
                    {
                        LogTextBox.SelectionStart = LogTextBox.Text.Length;
                        LogTextBox.ScrollToCaret();
                    }
                }
            }
            catch (Exception ex)
            {
                LogTextBox.AppendText($"[UI ERROR] Failad to update log:{ex.Message}\r\n");
            }
        }
    }
}
