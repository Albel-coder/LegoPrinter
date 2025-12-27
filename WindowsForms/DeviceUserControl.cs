using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WindowsForms
{
    public partial class DeviceUserControl : UserControl
    {
        private PrinterController printerController;
        private int lastLogCount = 0;
        private readonly object updateLock = new object();
        private bool isUpdating = false;

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
            if (!printerController.IsPrinterConnect())
            {
                this.connectButton.Enabled = false;
                this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(227)))), ((int)(((byte)(235)))), ((int)(((byte)(12)))));
                this.connectButton.Text = "connecting...";

                try
                {
                    bool isConnected = await Task.Run(() => printerController.Connect());

                    if (isConnected)
                    {
                        this.connectButton.Enabled = true;
                        this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(234)))), ((int)(((byte)(84)))), ((int)(((byte)(85)))));
                        this.connectButton.Text = "disconnect";
                    }
                }
                catch (Exception)
                {
                    this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(29)))), ((int)(((byte)(175)))), ((int)(((byte)(30)))));
                    this.connectButton.Enabled = true;
                    this.connectButton.Text = "Connect";
                    throw;
                }

                if (!printerController.IsPrinterConnect())
                {
                    this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(29)))), ((int)(((byte)(175)))), ((int)(((byte)(30)))));
                    this.connectButton.Enabled = true;
                    this.connectButton.Text = "Connect";
                }
            }
            else
            {
                this.connectButton.Enabled = false;
                this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(227)))), ((int)(((byte)(235)))), ((int)(((byte)(12)))));
                this.connectButton.Text = "disconnecting...";
                try
                {
                    bool isDisconnected = await Task.Run(() => printerController.Disconnect());

                    if (isDisconnected)
                    {
                        this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(29)))), ((int)(((byte)(175)))), ((int)(((byte)(30)))));
                        this.connectButton.Enabled = true;
                        this.connectButton.Text = "Connect";
                    }
                }
                catch (Exception)
                {
                    this.connectButton.Enabled = true;
                    this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(234)))), ((int)(((byte)(84)))), ((int)(((byte)(85)))));
                    this.connectButton.Text = "disconnect";
                    throw;
                }

                if (printerController.IsPrinterConnect())
                {
                    this.connectButton.Enabled = true;
                    this.connectButton.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(234)))), ((int)(((byte)(84)))), ((int)(((byte)(85)))));
                    this.connectButton.Text = "disconnect";
                }
            }
        }
        private void logTimer_Tick(object sender, EventArgs e)
        {
            UpdateConsoleDisplay();
        }
        private void UpdateConsoleDisplay()
        {
            if (isUpdating) return;

            lock(updateLock)
            {
                if (isUpdating) return;
                isUpdating = true;
            }

            try
            {
                UpdateConsoleDisplayInternal();
            }
            finally
            {
                lock(updateLock)
                {
                    isUpdating = false;
                }
            }
        }

        private void UpdateConsoleDisplayInternal()
        {
            try
            {
                int currentLogCount = printerController.GetLogCount();

                if (currentLogCount < lastLogCount)
                {
                    ClearTextBoxSafe();
                    lastLogCount = 0;

                    if (currentLogCount > 0)
                    {
                        AppendNewLogs(0, currentLogCount);
                        lastLogCount = currentLogCount;
                    }

                    return;
                }

                if (currentLogCount > lastLogCount)
                {
                    int newLogs = currentLogCount - lastLogCount;
                    AppendNewLogs(lastLogCount, newLogs);
                    lastLogCount = currentLogCount;
                }
            }
            catch (Exception)
            {
                throw;
            }
        }

        private void AppendNewLogs(int startIndex, int count)
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
                    // Быстрая вставка с приостановкой обновления
                    textBoxConsole.SuspendLayout();
                    textBoxConsole.AppendText(text);

                    // Автопрокрутка если включена
                    if (AutoScrollEnabled)
                    {
                        textBoxConsole.SelectionStart = textBoxConsole.TextLength;
                        textBoxConsole.ScrollToCaret();
                    }

                    textBoxConsole.ResumeLayout();

                    // Ограничиваем размер для производительности
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
            // Ограничиваем лог для производительности, но НЕ очищаем полностью
            // Просто удаляем старые строки если их слишком много
            const int MAX_LINES = 2000;
            const int KEEP_LINES = 1500;

            if (textBoxConsole.Lines.Length > MAX_LINES)
            {
                var lines = textBoxConsole.Lines;
                int removeCount = lines.Length - KEEP_LINES;

                if (removeCount > 0)
                {
                    // Создаем новые строки без старых
                    var newLines = new string[KEEP_LINES];
                    Array.Copy(lines, removeCount, newLines, 0, KEEP_LINES);

                    // Обновляем TextBox
                    textBoxConsole.Lines = newLines;

                    // Корректируем счетчик (примерно)
                    lastLogCount = Math.Max(0, lastLogCount - removeCount);
                }
            }
        }
        public bool AutoScrollEnabled { get; set; } = true;

        public void ClearLogs()
        {
            printerController.ClearLog();
            ClearTextBoxSafe();
            lastLogCount = 0;
        }
    }
}
