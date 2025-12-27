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
        private readonly object logSyncLock = new object();
        private bool isUpdatingLogs = false;
        private readonly StringBuilder logBatchBuffer = new StringBuilder(4096);
        private readonly Queue<string> pendingLogUpdates = new Queue<string>();

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
                    throw;
                }
                finally
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

                    throw;
                }
                finally
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
            if (isUpdatingLogs) return;

            lock (logSyncLock)
            {
                if (isUpdatingLogs) return;
                isUpdatingLogs = true;
            }

            try
            {
                int currentLogCount = printerController.GetLogCount();

                // Если количество логов уменьшилось - была очистка
                if (currentLogCount < lastLogCount)
                {
                    textBoxConsole.Clear();
                    lastLogCount = 0;

                    // Если после очистки есть логи, читаем их все
                    if (currentLogCount > 0)
                    {
                        lastLogCount = currentLogCount;
                        ReadAndAppendLogs(0, currentLogCount);
                    }
                }
                // Если появились новые логи
                else if (currentLogCount > lastLogCount)
                {
                    int newLogsCount = currentLogCount - lastLogCount;
                    ReadAndAppendLogs(lastLogCount, newLogsCount);
                    lastLogCount = currentLogCount;
                }

                // Автопрокрутка если включена
                if (AutoScrollEnabled)
                {
                    textBoxConsole.SelectionStart = textBoxConsole.Text.Length;
                    textBoxConsole.ScrollToCaret();
                }

                // Ограничение размера лога для производительности
                LimitLogSize();
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UI ERROR] Failed to update log: {ex.Message}");

                // В случае ошибки добавляем сообщение в лог
                if (textBoxConsole.InvokeRequired)
                {
                    textBoxConsole.BeginInvoke((MethodInvoker)delegate
                    {
                        textBoxConsole.AppendText($"[UI ERROR] {ex.Message}\r\n");
                    });
                }
                else
                {
                    textBoxConsole.AppendText($"[UI ERROR] {ex.Message}\r\n");
                }
            }
            finally
            {
                lock (logSyncLock)
                {
                    isUpdatingLogs = false;
                }
            }
        }

        private void ReadAndAppendLogs(int startIndex, int count)
        {
            // Используем буфер для накопления строк
            logBatchBuffer.Clear();

            for (int i = 0; i < count; i++)
            {
                try
                {
                    string logEntry = printerController.GetLogEntry(startIndex + i);

                    if (!string.IsNullOrEmpty(logEntry))
                    {
                        // Проверяем категорию (опционально)
                        if (ShouldDisplayLog(logEntry))
                        {
                            logBatchBuffer.AppendLine(logEntry);
                        }
                    }
                }
                catch (Exception ex)
                {
                    Debug.WriteLine($"Error reading log entry {startIndex + i}: {ex.Message}");
                }
            }

            // Если есть что добавлять
            if (logBatchBuffer.Length > 0)
            {
                string logText = logBatchBuffer.ToString();

                // Добавляем в очередь для UI обновления
                lock (pendingLogUpdates)
                {
                    pendingLogUpdates.Enqueue(logText);
                }

                // Выполняем обновление UI в правильном потоке
                if (textBoxConsole.InvokeRequired)
                {
                    textBoxConsole.BeginInvoke((MethodInvoker)UpdateTextBoxFromQueue);
                }
                else
                {
                    UpdateTextBoxFromQueue();
                }
            }
        }

        private void UpdateTextBoxFromQueue()
        {
            string[] updates;
            lock (pendingLogUpdates)
            {
                if (pendingLogUpdates.Count == 0) return;
                updates = pendingLogUpdates.ToArray();
                pendingLogUpdates.Clear();
            }

            // Приостанавливаем обновление для быстрой вставки
            textBoxConsole.SuspendLayout();

            try
            {
                var selectionStart = textBoxConsole.SelectionStart;
                var selectionLength = textBoxConsole.SelectionLength;
                var scrollToBottom = textBoxConsole.SelectionStart == textBoxConsole.TextLength;

                // Добавляем все накопленные логи одной операцией
                foreach (var update in updates)
                {
                    textBoxConsole.AppendText(update);
                }

                // Восстанавливаем позицию прокрутки
                if (scrollToBottom)
                {
                    textBoxConsole.SelectionStart = textBoxConsole.TextLength;
                    textBoxConsole.ScrollToCaret();
                }
                else
                {
                    textBoxConsole.SelectionStart = selectionStart;
                    textBoxConsole.SelectionLength = selectionLength;
                }
            }
            finally
            {
                textBoxConsole.ResumeLayout();
            }
        }

        private void LimitLogSize()
        {
            // Ограничиваем лог для производительности (например, 2000 строк)
            const int MAX_LOG_LINES = 2000;

            if (textBoxConsole.Lines.Length > MAX_LOG_LINES)
            {
                textBoxConsole.SuspendLayout();
                try
                {
                    var lines = textBoxConsole.Lines;
                    int removeCount = lines.Length - MAX_LOG_LINES;

                    // Удаляем старые строки
                    var newLines = lines.Skip(removeCount).ToArray();
                    textBoxConsole.Lines = newLines;

                    // Корректируем lastLogCount
                    lastLogCount = Math.Max(0, lastLogCount - removeCount);
                }
                finally
                {
                    textBoxConsole.ResumeLayout();
                }
            }
        }

        // Опционально: фильтрация по категориям
        private bool ShouldDisplayLog(string logEntry)
        {
            // Если все категории включены - пропускаем все
            if (enabledCategories == LogCategory.All) return true;

            // Определяем категорию лога по префиксу
            if (logEntry.Contains("[ERROR]"))
                return (enabledCategories & LogCategory.Error) != 0;
            if (logEntry.Contains("[WARNING]"))
                return (enabledCategories & LogCategory.Warning) != 0;
            if (logEntry.Contains("[INFO]"))
                return (enabledCategories & LogCategory.Info) != 0;
            if (logEntry.Contains("[DEBUG]"))
                return (enabledCategories & LogCategory.Debug) != 0;
            if (logEntry.Contains("[MOTOR]"))
                return (enabledCategories & LogCategory.Motor) != 0;
            if (logEntry.Contains("[ENCODER]"))
                return (enabledCategories & LogCategory.Encoder) != 0;
            if (logEntry.Contains("[BLUETOOTH]"))
                return (enabledCategories & LogCategory.Bluetooth) != 0;
            if (logEntry.Contains("[PROFILE]"))
                return (enabledCategories & LogCategory.Profile) != 0;
            if (logEntry.Contains("[PERFORMANCE]"))
                return (enabledCategories & LogCategory.Performance) != 0;
            if (logEntry.Contains("[COMMAND]"))
                return (enabledCategories & LogCategory.Command) != 0;

            // Если категория не определена - показываем
            return true;
        }

        // Свойство для автоматической прокрутки
        public bool AutoScrollEnabled { get; set; } = true;

        // Методы для управления логами
        public void ClearLogs()
        {
            printerController.ClearLog();

            if (textBoxConsole.InvokeRequired)
            {
                textBoxConsole.Invoke((MethodInvoker)delegate
                {
                    textBoxConsole.Clear();
                    lastLogCount = 0;
                });
            }
            else
            {
                textBoxConsole.Clear();
                lastLogCount = 0;
            }
        }

        public void SetLogCategories(uint categories)
        {
            enabledCategories = (LogCategory)categories;
        }

        public uint GetLogCategories()
        {
            return (uint)enabledCategories;
        }

        // Простые методы для логирования из C# (опционально)
        public void AddLog(string message)
        {
            // Если нужно логировать что-то из C#, можно добавить в textBox напрямую
            if (textBoxConsole.InvokeRequired)
            {
                textBoxConsole.BeginInvoke((MethodInvoker)delegate
                {
                    textBoxConsole.AppendText($"[CSHARP] {DateTime.Now:HH:mm:ss.fff} {message}\r\n");
                });
            }
            else
            {
                textBoxConsole.AppendText($"[CSHARP] {DateTime.Now:HH:mm:ss.fff} {message}\r\n");
            }
        }
    }
}
