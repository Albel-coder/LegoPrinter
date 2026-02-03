using System;
using System.Drawing;
using System.Windows.Forms;
using LPStudio.Services;

namespace LPStudio
{
    public partial class UpdateDialog : Form
    {
        private readonly UpdateInfo _updateInfo;
        private readonly GitHubUpdateService _updateService;
        private bool _autoUpdateEnabled;
        private string _skipVersion; // Теперь будет хранить полную версию (4 числа)
        private DateTime _postponeUntil;

        // UI элементы
        private Label _lblTitle;
        private Label _lblCurrentVersion;
        private Label _lblLatestVersion;
        private RichTextBox _txtReleaseNotes;
        private CheckBox _chkAutoUpdate;
        private Button _btnUpdate;
        private Button _btnLater;
        private Button _btnSkip;
        private ProgressBar _progressBar;
        private Label _lblProgress;

        public UpdateDialog(UpdateInfo updateInfo, GitHubUpdateService updateService)
        {
            _updateInfo = updateInfo ?? throw new ArgumentNullException(nameof(updateInfo));
            _updateService = updateService ?? throw new ArgumentNullException(nameof(updateService));

            // Загружаем временные настройки
            LoadTempSettings();

            InitializeForm();
            InitializeUI();
        }

        private void InitializeForm()
        {
            // Настройки формы
            this.Text = "Доступно обновление";
            this.Size = new Size(500, 450);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.BackColor = SystemColors.Window;
        }

        private void InitializeUI()
        {
            // Формируем полные версии для отображения (4 числа)
            string currentFullVersion = $"{_updateInfo.CurrentVersion}.{_updateInfo.CurrentBuild}";
            string latestFullVersion = $"{_updateInfo.LatestVersion}.{_updateInfo.LatestBuild}";

            // Заголовок
            _lblTitle = new Label
            {
                Text = $"Доступна новая версия {latestFullVersion}",
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                Location = new Point(20, 20),
                Size = new Size(450, 30),
                ForeColor = Color.SteelBlue
            };

            // Текущая версия
            _lblCurrentVersion = new Label
            {
                Text = $"Текущая версия: {currentFullVersion}",
                Location = new Point(20, 60),
                Size = new Size(200, 20)
            };

            // Последняя версия
            _lblLatestVersion = new Label
            {
                Text = $"Новая версия: {latestFullVersion}",
                Location = new Point(250, 60),
                Size = new Size(200, 20),
                ForeColor = Color.Green,
                Font = new Font("Segoe UI", 9, FontStyle.Bold)
            };

            // Убираем ReleaseName, так как его нет в UpdateInfo
            // Вместо этого можем показать productVersion
            var lblProductVersion = new Label
            {
                Text = !string.IsNullOrEmpty(_updateInfo.ProductVersion)
                    ? $"Версия принтера: {_updateInfo.ProductVersion}"
                    : "",
                Font = new Font("Segoe UI", 9, FontStyle.Italic),
                Location = new Point(20, 90),
                Size = new Size(450, 20),
                ForeColor = Color.Gray
            };

            // Описание изменений
            var lblReleaseNotes = new Label
            {
                Text = "Изменения в этой версии:",
                Location = new Point(20, 120),
                Size = new Size(150, 20),
                Font = new Font("Segoe UI", 9, FontStyle.Bold)
            };

            // Текст изменений
            _txtReleaseNotes = new RichTextBox
            {
                Text = !string.IsNullOrEmpty(_updateInfo.ReleaseNotes)
                    ? _updateInfo.ReleaseNotes
                    : "Нет информации об изменениях",
                Location = new Point(20, 145),
                Size = new Size(440, 150),
                ReadOnly = true,
                BorderStyle = BorderStyle.FixedSingle,
                ScrollBars = RichTextBoxScrollBars.Vertical
            };

            // Прогресс-бар (скрыт по умолчанию)
            _progressBar = new ProgressBar
            {
                Location = new Point(20, 310),
                Size = new Size(440, 20),
                Visible = false,
                Style = ProgressBarStyle.Continuous
            };

            // Текст прогресса
            _lblProgress = new Label
            {
                Location = new Point(20, 335),
                Size = new Size(440, 20),
                Visible = false,
                TextAlign = ContentAlignment.MiddleCenter
            };

            // Автоматическое обновление
            _chkAutoUpdate = new CheckBox
            {
                Text = "Всегда проверять обновления при запуске",
                Location = new Point(20, 310),
                Size = new Size(300, 20),
                Checked = _autoUpdateEnabled
            };
            _chkAutoUpdate.CheckedChanged += (s, e) =>
            {
                _autoUpdateEnabled = _chkAutoUpdate.Checked;
                SaveTempSettings();
            };

            // Кнопка "Обновить"
            _btnUpdate = new Button
            {
                Text = "Установить обновление",
                Location = new Point(20, 350),
                Size = new Size(150, 35),
                BackColor = Color.SteelBlue,
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                FlatStyle = FlatStyle.Flat
            };
            _btnUpdate.Click += BtnUpdate_Click;

            // Кнопка "Позже"
            _btnLater = new Button
            {
                Text = "Напомнить позже",
                Location = new Point(180, 350),
                Size = new Size(120, 35),
                FlatStyle = FlatStyle.Flat
            };
            _btnLater.Click += BtnLater_Click;

            // Кнопка "Пропустить"
            _btnSkip = new Button
            {
                Text = "Пропустить версию",
                Location = new Point(310, 350),
                Size = new Size(150, 35),
                FlatStyle = FlatStyle.Flat
            };
            _btnSkip.Click += BtnSkip_Click;

            // Добавляем элементы на форму
            this.Controls.AddRange(new Control[]
            {
                _lblTitle, _lblCurrentVersion, _lblLatestVersion, lblProductVersion,
                lblReleaseNotes, _txtReleaseNotes, _progressBar, _lblProgress,
                _chkAutoUpdate, _btnUpdate, _btnLater, _btnSkip
            });
        }

        private void LoadTempSettings()
        {
            try
            {
                // Временная загрузка настроек из файла
                string settingsPath = System.IO.Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "LPStudio", "update_settings.txt");

                if (System.IO.File.Exists(settingsPath))
                {
                    var lines = System.IO.File.ReadAllLines(settingsPath);
                    foreach (var line in lines)
                    {
                        var parts = line.Split('=');
                        if (parts.Length == 2)
                        {
                            string key = parts[0].Trim();
                            string value = parts[1].Trim();

                            switch (key)
                            {
                                case "CheckForUpdates":
                                    _autoUpdateEnabled = bool.Parse(value);
                                    break;
                                case "SkipVersion":
                                    _skipVersion = value;
                                    break;
                                case "PostponeUntil":
                                    _postponeUntil = DateTime.Parse(value);
                                    break;
                            }
                        }
                    }
                }
            }
            catch
            {
                // Значения по умолчанию
                _autoUpdateEnabled = true;
                _skipVersion = "";
                _postponeUntil = DateTime.MinValue;
            }
        }

        private void SaveTempSettings()
        {
            try
            {
                string settingsPath = System.IO.Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "LPStudio", "update_settings.txt");

                System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(settingsPath));

                string content = $"CheckForUpdates={_autoUpdateEnabled}\n" +
                               $"SkipVersion={_skipVersion}\n" +
                               $"PostponeUntil={_postponeUntil:O}";

                System.IO.File.WriteAllText(settingsPath, content);
            }
            catch
            {
                // Игнорируем ошибки
            }
        }

        private async void BtnUpdate_Click(object sender, EventArgs e)
        {
            try
            {
                // Логируем запуск обновления с полными версиями
                string currentFull = $"{_updateInfo.CurrentVersion}.{_updateInfo.CurrentBuild}";
                string latestFull = $"{_updateInfo.LatestVersion}.{_updateInfo.LatestBuild}";
                Console.WriteLine($"[UpdateDialog] Запуск обновления с {currentFull} на {latestFull}");

                // Скрываем кнопки и показываем прогресс
                SetUpdateInProgress(true);

                // Скачиваем и устанавливаем обновление
                _updateService.StartUpdateProcess(_updateInfo);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ошибка при обновлении: {ex.Message}",
                    "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Error);
                SetUpdateInProgress(false);
            }
        }

        private void BtnLater_Click(object sender, EventArgs e)
        {
            // Откладываем обновление на 1 день
            _postponeUntil = DateTime.Now.AddDays(1);
            SaveTempSettings();

            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }

        private void BtnSkip_Click(object sender, EventArgs e)
        {
            // Пропускаем эту версию - сохраняем полную версию с build
            string latestFullVersion = $"{_updateInfo.LatestVersion}.{_updateInfo.LatestBuild}";
            _skipVersion = latestFullVersion;
            SaveTempSettings();

            Console.WriteLine($"[UpdateDialog] Версия {_skipVersion} пропущена");

            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }

        private void SetUpdateInProgress(bool inProgress)
        {
            _btnUpdate.Enabled = !inProgress;
            _btnLater.Enabled = !inProgress;
            _btnSkip.Enabled = !inProgress;
            _chkAutoUpdate.Visible = !inProgress;

            _progressBar.Visible = inProgress;
            _lblProgress.Visible = inProgress;

            if (inProgress)
            {
                _lblProgress.Text = "Подготовка к обновлению...";
                _progressBar.Style = ProgressBarStyle.Marquee;
            }
        }

        // Метод для обновления прогресса
        public void UpdateProgress(double percentage, string status)
        {
            if (InvokeRequired)
            {
                Invoke(new Action<double, string>(UpdateProgress), percentage, status);
                return;
            }

            _progressBar.Value = (int)percentage;
            _lblProgress.Text = status;
        }
    }
}
