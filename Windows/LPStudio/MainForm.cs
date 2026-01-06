using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using LPStudio.Services;

namespace LPStudio
{
    public partial class MainForm : Form
    {
        private readonly Dictionary<string, UserControl> tabs = new Dictionary<string, UserControl>();
        private List<Panel> panels = new List<Panel>();
        private UserControl currentTab;

        private bool isDragging = false;
        private Point dragStartPoint = Point.Empty;

        private GitHubUpdateService _updateService;
        private bool _updateChecked = false;
        private System.Threading.Timer _updateTimer;

        private NotifyIcon notifyIcon;
        public MainForm()
        {
            InitializeComponent();
            InitializeNotifyIcon();
            SetupTabs();

            this.Text = string.Empty;
            this.ControlBox = false;
            this.MaximizedBounds = Screen.FromHandle(this.Handle).WorkingArea;

            SubscribeAllControls(this);

            string githubOwner = "Albel-coder";
            string githubRepo = "LegoPrinter";

            _updateService = new GitHubUpdateService(githubOwner, githubRepo);

            this.Shown += MainForm_Shown;
        }

        private async void MainForm_Shown(object sender, EventArgs e)
        {
            Console.WriteLine("start MainForm_Shown");
            await Task.Delay(3000);
            await CheckForUpdatesAsync();
            Console.WriteLine("end MainForm_Shown");
        }

        private void InitializeNotifyIcon()
        {
            notifyIcon = new NotifyIcon
            {
                Icon = SystemIcons.Information,
                Visible = true,
                BalloonTipTitle = "Обновления",
                BalloonTipText = "У вас установлена последняя версия",
                BalloonTipIcon = ToolTipIcon.Info
            };
        }
        private async Task CheckForUpdatesAsync(bool manualCheck = true)
        {
            try
            {
                Console.WriteLine($"Начало проверки обновлений (ручная проверка: {manualCheck})");

                if (_updateService == null)
                {
                    Console.WriteLine("Сервис обновлений не настроен");
                    MessageBox.Show("Сервис обновлений не настроен",
                        "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }                

                var updateInfo = await _updateService.CheckForUpdatesAsync(manualCheck);

                Console.WriteLine($"Результат проверки: {(updateInfo == null ? "null" : "не null")}");
                if (updateInfo != null)
                {
                    Console.WriteLine($"Доступно обновление: {updateInfo.IsAvailable}");
                    Console.WriteLine($"Текущая версия: {updateInfo.CurrentVersion}");
                    Console.WriteLine($"Новая версия: {updateInfo.LatestVersion}");
                    Console.WriteLine($"URL для скачивания: {updateInfo.DownloadUrl ?? "null"}");
                    Console.WriteLine($"Имя файла: {updateInfo.AssetName ?? "null"}");
                }

                if (updateInfo == null)
                {
                    if (manualCheck)
                        MessageBox.Show("Не удалось проверить обновления.", "Ошибка");
                    return;
                }

                if (!updateInfo.IsAvailable)
                {
                    if (manualCheck)
                        notifyIcon.ShowBalloonTip(5000);
                    return;
                }

                Console.WriteLine("Показ диалога обновления...");
                ShowUpdateDialog(updateInfo);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка при проверке обновлений: {ex.Message}");
                Console.WriteLine($"StackTrace: {ex.StackTrace}");

                if (manualCheck)
                {
                    MessageBox.Show($"Error: {ex.Message}", "Error");
                }
            }
        }

        private void ShowUpdateDialog(UpdateInfo updateInfo)
        {
            Console.WriteLine("show update dialog");
            var dialog = new UpdateDialog(updateInfo, _updateService);
            dialog.ShowDialog();
        }

        [DllImport("user32.DLL", EntryPoint = "ReleaseCapture")]
        private extern static void ReleaseCapture();

        [DllImport("user32.DLL", EntryPoint = "SendMessage")]
        private extern static void SendMessage(System.IntPtr hWindow, int wMessage, int wParameter, int lParameter);

        private void SetupTabs()
        {
            tabs.Add("DeviceUserControl", new DeviceUserControl());
            tabs.Add("PreviewUserControl", new PreviewUserControl());
            tabs.Add("PrepareUserControl", new PrepareUserControl());
            tabs.Add("CalibrationUserControl", new CalibrationUserControl());

            foreach(KeyValuePair<string, UserControl> tab in tabs)
            {
                tab.Value.Dock = DockStyle.Fill;
                tab.Value.Visible = false;
                contentPanel.Controls.Add(tab.Value);
            }

            ShowTab("DeviceUserControl");
        }
        private void ShowTab(string tabKey)
        {
            if (currentTab != null)
            {
                currentTab.Visible = false;
            }

            if (tabs.ContainsKey(tabKey))
            {
                currentTab = tabs[tabKey];
                currentTab.Visible = true;
            }
        }
        private void MainForm_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        private void SubscribeAllControls(Control control)
        {
            foreach (Control childControl in control.Controls)
            {
                // Launch buttons and other interactive elements
                if (!(childControl is Button) &&
                    !(childControl is TextBox) &&
                    !(childControl is ComboBox) &&
                    !(childControl is CheckBox) &&
                    !(childControl is RadioButton))
                {
                    childControl.MouseDown += MainForm_MouseDown;
                }

                if (childControl.HasChildren)
                {
                    SubscribeAllControls(childControl);
                }
            }
        }

        private void exitButton_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void buttonPreview_Click(object sender, EventArgs e)
        {
            ShowTab("PreviewUserControl");            
        }

        private void restoreWindowButton_Click(object sender, EventArgs e)
        {
            if (WindowState == FormWindowState.Normal)
            {
                this.WindowState = FormWindowState.Maximized;
                this.restoreWindowButton.Image = global::LPStudio.Properties.Resources.restoreImage32x32;
            }
            else
            {
                this.WindowState = FormWindowState.Normal;
                this.restoreWindowButton.Image = global::LPStudio.Properties.Resources.maximizeWindowImage32x32;
            }
        }
        private void minimizeWindowButton_Click(object sender, EventArgs e)
        {
            this.WindowState = FormWindowState.Minimized;
        }
        private void buttonHome_Click(object sender, EventArgs e)
        {
            ShowTab("DeviceUserControl");
        }
        private void buttonPrepare_Click(object sender, EventArgs e)
        {
            ShowTab("PrepareUserControl");
        }

        private void buttonDevice_Click(object sender, EventArgs e)
        {
            ShowTab("DeviceUserControl");
        }
        private void buttonCalibration_Click(object sender, EventArgs e)
        {
            ShowTab("CalibrationUserControl");
        }
    }
}