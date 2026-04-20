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
        private string _skipVersion; // Will now store the full version (4 numbers)
        private DateTime _postponeUntil;

        // UI elements
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

            // Load temporary settings
            LoadTempSettings();

            InitializeForm();
            InitializeUI();
        }

        private void InitializeForm()
        {
            // Form settings
            this.Text = "Update available";
            this.Size = new Size(500, 450);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.BackColor = SystemColors.Window;
        }

        private void InitializeUI()
        {
            // Generate full versions for display (4 numbers)
            string currentFullVersion = $"{_updateInfo.CurrentVersion}.{_updateInfo.CurrentBuild}";
            string latestFullVersion = $"{_updateInfo.LatestVersion}.{_updateInfo.LatestBuild}";

            // Title
            _lblTitle = new Label
            {
                Text = $"New version available {latestFullVersion}",
                Font = new Font("Segoe UI", 12, FontStyle.Bold),
                Location = new Point(20, 20),
                Size = new Size(450, 30),
                ForeColor = Color.SteelBlue
            };

            // Current version
            _lblCurrentVersion = new Label
            {
                Text = $"Current version: {currentFullVersion}",
                Location = new Point(20, 60),
                Size = new Size(200, 20)
            };

            // Latest version
            _lblLatestVersion = new Label
            {
                Text = $"New version: {latestFullVersion}",
                Location = new Point(250, 60),
                Size = new Size(200, 20),
                ForeColor = Color.Green,
                Font = new Font("Segoe UI", 9, FontStyle.Bold)
            };

            // Remove ReleaseName, since it's not in UpdateInfo
            // We can show productVersion instead
            var lblProductVersion = new Label
            {
                Text = !string.IsNullOrEmpty(_updateInfo.ProductVersion)
                    ? $"Printer version: {_updateInfo.ProductVersion}"
                    : "",
                Font = new Font("Segoe UI", 9, FontStyle.Italic),
                Location = new Point(20, 90),
                Size = new Size(450, 20),
                ForeColor = Color.Gray
            };

            // Description of changes
            var lblReleaseNotes = new Label
            {
                Text = "Changes in this version:",
                Location = new Point(20, 120),
                Size = new Size(150, 20),
                Font = new Font("Segoe UI", 9, FontStyle.Bold)
            };

            // Change text
            _txtReleaseNotes = new RichTextBox
            {
                Text = !string.IsNullOrEmpty(_updateInfo.ReleaseNotes)
                    ? _updateInfo.ReleaseNotes
                    : "No information about changes",
                Location = new Point(20, 145),
                Size = new Size(440, 150),
                ReadOnly = true,
                BorderStyle = BorderStyle.FixedSingle,
                ScrollBars = RichTextBoxScrollBars.Vertical
            };

            // Progress bar (hidden by default)
            _progressBar = new ProgressBar
            {
                Location = new Point(20, 310),
                Size = new Size(440, 20),
                Visible = false,
                Style = ProgressBarStyle.Continuous
            };

            // Progress text
            _lblProgress = new Label
            {
                Location = new Point(20, 335),
                Size = new Size(440, 20),
                Visible = false,
                TextAlign = ContentAlignment.MiddleCenter
            };

            // Automatic update
            _chkAutoUpdate = new CheckBox
            {
                Text = "Always check for updates when starting up",
                Location = new Point(20, 310),
                Size = new Size(300, 20),
                Checked = _autoUpdateEnabled
            };
            _chkAutoUpdate.CheckedChanged += (s, e) =>
            {
                _autoUpdateEnabled = _chkAutoUpdate.Checked;
                SaveTempSettings();
            };

            // "Refresh" button
            _btnUpdate = new Button
            {
                Text = "Install update",
                Location = new Point(20, 350),
                Size = new Size(150, 35),
                BackColor = Color.SteelBlue,
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 9, FontStyle.Bold),
                FlatStyle = FlatStyle.Flat
            };
            _btnUpdate.Click += BtnUpdate_Click;

            // "Later" button
            _btnLater = new Button
            {
                Text = "Напомнить позже",
                Location = new Point(180, 350),
                Size = new Size(120, 35),
                FlatStyle = FlatStyle.Flat
            };
            _btnLater.Click += BtnLater_Click;

            // Skip button
            _btnSkip = new Button
            {
                Text = "Skip version",
                Location = new Point(310, 350),
                Size = new Size(150, 35),
                FlatStyle = FlatStyle.Flat
            };
            _btnSkip.Click += BtnSkip_Click;

            // Add elements to the form
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
                // Temporarily loading settings from a file
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
                // Default values
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
                // Ignore errors
            }
        }

        private async void BtnUpdate_Click(object sender, EventArgs e)
        {
            try
            {
                // Logging the launch of an update with full versions
                string currentFull = $"{_updateInfo.CurrentVersion}.{_updateInfo.CurrentBuild}";
                string latestFull = $"{_updateInfo.LatestVersion}.{_updateInfo.LatestBuild}";
                Console.WriteLine($"[UpdateDialog] Starting update from {currentFull} to {latestFull}");

                // Hide the buttons and show the progress
                SetUpdateInProgress(true);

                // Download and install the update
                _updateService.StartUpdateProcess(_updateInfo);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error while updating: {ex.Message}",
                    "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                SetUpdateInProgress(false);
            }
        }

        private void BtnLater_Click(object sender, EventArgs e)
        {
            // Postpone the update for 1 day
            _postponeUntil = DateTime.Now.AddDays(1);
            SaveTempSettings();

            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }

        private void BtnSkip_Click(object sender, EventArgs e)
        {
            // Skip this version - save the full version with build
            string latestFullVersion = $"{_updateInfo.LatestVersion}.{_updateInfo.LatestBuild}";
            _skipVersion = latestFullVersion;
            SaveTempSettings();

            Console.WriteLine($"[UpdateDialog] Version {_skipVersion} skipped");

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
                _lblProgress.Text = "Preparing for the update...";
                _progressBar.Style = ProgressBarStyle.Marquee;
            }
        }

        // Method for updating progress
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
