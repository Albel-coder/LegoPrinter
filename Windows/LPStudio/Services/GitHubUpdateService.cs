using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace LPStudio.Services
{
    /// <summary>
    /// A service for checking and installing updates via GitHub
    /// </summary>
    public class GitHubUpdateService
    {
        // Constants
        private const string GITHUB_API_URL = "https://api.github.com";
        private const string DEFAULT_USER_AGENT = "LegoPrinter-App-Updater";

        // Settings
        private readonly string _owner;
        private readonly string _repo;
        private readonly string _userAgent;
        private readonly HttpClient _httpClient;

        // Cache for storing the last check
        private UpdateInfo _cachedUpdateInfo;
        private DateTime _lastCheckTime = DateTime.MinValue;
        private readonly TimeSpan _cacheDuration = TimeSpan.FromHours(1);

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="owner">Repository owner</param>
        /// <param name="repo">Repository name</param>
        public GitHubUpdateService(string owner, string repo)
        {
            _owner = owner ?? throw new ArgumentNullException(nameof(owner));
            _repo = repo ?? throw new ArgumentNullException(nameof(repo));
            _userAgent = $"{DEFAULT_USER_AGENT}/{UpdateHelper.GetCurrentVersion()}";

            // Configure HttpClient
            _httpClient = new HttpClient();
            _httpClient.DefaultRequestHeaders.UserAgent.ParseAdd(_userAgent);
            _httpClient.Timeout = TimeSpan.FromSeconds(30);

            // If needed, you can add a GitHub token to increase the limits
            // if (!string.IsNullOrEmpty(githubToken))
            // {
            //     _httpClient.DefaultRequestHeaders.Authorization = 
            //         new System.Net.Http.Headers.AuthenticationHeaderValue("token", githubToken);
            // }
        }

        /// <summary>
        /// Check for updates
        /// </summary>
        /// <param name="forceCheck">Force verification (ignore cache)</param>
        /// <returns>Update information</returns>
        public async Task<UpdateInfo> CheckForUpdatesAsync(bool forceCheck = false)
        {
            try
            {
                // Use cache if not forced check
                if (!forceCheck && DateTime.Now - _lastCheckTime < _cacheDuration && _cachedUpdateInfo != null)
                {
                    return _cachedUpdateInfo;
                }

                // Get information about the latest release
                var latestRelease = await GetLatestReleaseAsync();

                if (latestRelease == null)
                {
                    return CreateNoUpdateInfo();
                }

                // Get the current version
                var currentVersion = UpdateHelper.GetCurrentVersion();
                var latestVersion = UpdateHelper.CleanVersion(latestRelease.TagName);

                // Check if there is an update
                bool hasUpdate = UpdateHelper.IsVersionNewer(latestVersion, currentVersion);

                // We are looking for a suitable file for our platform
                var installerAsset = FindInstallerAsset(latestRelease.Assets);

                // Create update information
                var updateInfo = new UpdateInfo
                {
                    IsAvailable = hasUpdate,
                    CurrentVersion = currentVersion,
                    LatestVersion = latestVersion,
                    DownloadUrl = installerAsset?.BrowserDownloadUrl,
                    ReleaseNotes = latestRelease.Body,
                    ReleaseName = latestRelease.Name,
                    PublishedAt = latestRelease.PublishedAt,
                    IsPrerelease = latestRelease.Prerelease,
                    AssetName = installerAsset?.Name,
                    FileSize = installerAsset?.Size ?? 0
                };

                // Cache the result
                _cachedUpdateInfo = updateInfo;
                _lastCheckTime = DateTime.Now;

                return updateInfo;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[GitHubUpdateService] Ошибка проверки обновлений: {ex.Message}");
                return CreateNoUpdateInfo();
            }
        }

        /// <summary>
        /// Get information about the latest release
        /// </summary>
        private async Task<GitHubRelease> GetLatestReleaseAsync()
        {
            try
            {
                // Generate a URL to get the latest release
                string url = $"{GITHUB_API_URL}/repos/{_owner}/{_repo}/releases/latest";

                var response = await _httpClient.GetAsync(url);

                if (!response.IsSuccessStatusCode)
                {
                    // If we couldn't get the latest release, we try to get a list of all releases
                    return await GetLatestReleaseFallbackAsync();
                }

                var json = await response.Content.ReadAsStringAsync();
                return JsonConvert.DeserializeObject<GitHubRelease>(json);
            }
            catch (HttpRequestException ex)
            {
                return null;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[GitHubUpdateService] Ошибка получения релиза: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// Fallback method: get a list of all releases and take the first one
        /// </summary>
        private async Task<GitHubRelease> GetLatestReleaseFallbackAsync()
        {
            try
            {
                string url = $"{GITHUB_API_URL}/repo/{_owner}/{_repo}/releases";

                var response = await _httpClient.GetAsync(url);
                response.EnsureSuccessStatusCode();

                var json = await response.Content.ReadAsStringAsync();
                var releases = JsonConvert.DeserializeObject<GitHubRelease[]>(json);

                // Looking for the latest stable release
                var latestStable = releases?.FirstOrDefault(r => !r.Prerelease);
                return latestStable ?? releases?.FirstOrDefault();
            }
            catch
            {
                return null;
            }
        }

        /// <summary>
        /// Find the installer among the release files
        /// </summary>
        private GitHubAsset FindInstallerAsset(GitHubAsset[] assets)
        {
            if (assets == null || assets.Length == 0)
                return null;

            // Keywords for searching Windows Installer
            string[] windowsKeywords = { "windows", "win", ".exe", ".msi", "setup", "installer" };
            string[] excludeKeywords = { "android", "macos", "linux", ".dmg", ".deb", ".rpm", ".apk" };

            // Search in order of priority:

            // 1. We are looking for a file with an explicit indication of Windows and architecture
            foreach (var asset in assets)
            {
                var name = asset.Name.ToLower();

                // Skip files from other platforms
                if (excludeKeywords.Any(k => name.Contains(k)))
                    continue;

                // Check the architecture (if specified)
                bool is64Bit = Environment.Is64BitProcess;
                bool hasCorrectArchitecture =
                    (is64Bit && (name.Contains("x64") || name.Contains("64") || name.Contains("x86_64"))) ||
                    (!is64Bit && (name.Contains("x86") || name.Contains("32") || name.Contains("i386")));

                // If the file contains Windows keywords and the correct architecture (or no architecture is specified)
                if (windowsKeywords.Any(k => name.Contains(k)) &&
                    (hasCorrectArchitecture || !name.Contains("x64") && !name.Contains("x86")))
                {
                    return asset;
                }
            }

            // 2. Search for any .exe or .msi file
            foreach (var asset in assets)
            {
                var name = asset.Name.ToLower();

                if (excludeKeywords.Any(k => name.Contains(k)))
                    continue;

                if (name.EndsWith(".exe") || name.EndsWith(".msi"))
                {
                    return asset;
                }
            }

            // 3. Look for a ZIP archive with Windows in the name
            foreach (var asset in assets)
            {
                var name = asset.Name.ToLower();

                if (excludeKeywords.Any(k => name.Contains(k)))
                    continue;

                if ((name.Contains("windows") || name.Contains("win")) && name.EndsWith(".zip"))
                {
                    return asset;
                }
            }

            // 4. Return the first file if nothing was found
            return assets.FirstOrDefault();
        }

        /// <summary>
        /// Create information about the lack of updates
        /// </summary>
        private UpdateInfo CreateNoUpdateInfo()
        {
            return new UpdateInfo
            {
                IsAvailable = false,
                CurrentVersion = UpdateHelper.GetCurrentVersion(),
                LatestVersion = UpdateHelper.GetCurrentVersion()
            };
        }

        /// <summary>
        /// Start the update process
        /// </summary>
        public void StartUpdateProcess(UpdateInfo updateInfo)
        {
            if (updateInfo == null)
                throw new ArgumentNullException(nameof(updateInfo));

            if (!updateInfo.IsAvailable)
                throw new InvalidOperationException("Обновление недоступно");

            if (string.IsNullOrEmpty(updateInfo.DownloadUrl))
                throw new InvalidOperationException("URL для скачивания не указан");

            // Check the existence of Updater
            if (!UpdateHelper.UpdaterExists())
            {
                MessageBox.Show(
                    "Программа обновления не найдена. Пожалуйста, обновите приложение вручную.",
                    "Ошибка обновления",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            try
            {
                Console.WriteLine($"=== Запуск процесса обновления ===");
                Console.WriteLine($"Current PID: {Process.GetCurrentProcess().Id}");

                // Generate command line arguments for Updater
                var arguments = BuildUpdaterArguments(updateInfo);
                Console.WriteLine($"Аргументы Updater: {arguments}");

                // Launch Updater
                var updaterPath = UpdateHelper.GetUpdaterPath();
                Console.WriteLine($"Путь к Updater: {updaterPath}");
                Console.WriteLine($"Exists: {File.Exists(updaterPath)}");

                // Check the arguments in more detail
                Console.WriteLine($"\nДетали аргументов:");
                Console.WriteLine($"App Exe: {Application.ExecutablePath}");
                Console.WriteLine($"App Dir: {Application.StartupPath}");
                Console.WriteLine($"Download URL: {updateInfo.DownloadUrl}");
                Console.WriteLine($"Version: {updateInfo.LatestVersion}");
                Console.WriteLine($"Asset Name: {updateInfo.AssetName}");

                // Create a temporary file with logs
                var tempLogFile = Path.Combine(Path.GetTempPath(), $"updater_launch_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
                File.WriteAllText(tempLogFile,
                    $"Updater Path: {updaterPath}\n" +
                    $"Arguments: {arguments}\n" +
                    $"Time: {DateTime.Now}");

                Console.WriteLine($"Log file created: {tempLogFile}");

                // Run Updater with detailed logging
                var process = UpdateHelper.StartProcess(updaterPath, arguments, false);

                if (process == null)
                {
                    Console.WriteLine("Process.Start вернул null!");
                    throw new Exception("Не удалось запустить процесс Updater");
                }

                Console.WriteLine($"Updater запущен, PID: {process.Id}");
                Console.WriteLine($"Process HasExited: {process.HasExited}");

                // Give Updater some time to start
                Thread.Sleep(1000);

                if (process.HasExited)
                {
                    Console.WriteLine($"Updater завершился с кодом: {process.ExitCode}");
                    throw new Exception($"Updater завершился сразу после запуска. Код: {process.ExitCode}");
                }

                Console.WriteLine("Updater успешно запущен, закрываю основное приложение...");

                // Close the current application
                Application.Exit();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка запуска Updater: {ex.Message}");
                Console.WriteLine($"StackTrace: {ex.StackTrace}");

                MessageBox.Show(
                    $"Не удалось запустить процесс обновления: {ex.Message}\n\nПроверьте консоль для деталей.",
                    "Ошибка",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// Generate arguments for Updater
        /// </summary>
        private string BuildUpdaterArguments(UpdateInfo updateInfo)
        {
            var args = new StringBuilder();

            // Basic parameters
            args.Append($"--app-exe \"{Application.ExecutablePath}\" ");
            args.Append($"--app-dir \"{Application.StartupPath}\" ");
            args.Append($"--download-url \"{updateInfo.DownloadUrl}\" ");
            args.Append($"--version \"{updateInfo.LatestVersion}\" ");
            args.Append($"--wait-pid {Process.GetCurrentProcess().Id} ");

            // Additional parameters
            if (!string.IsNullOrEmpty(updateInfo.AssetName))
                args.Append($"--asset-name \"{updateInfo.AssetName}\" ");

            return args.ToString();
        }

        /// <summary>
        /// Download the update file directly (without Updater)
        /// </summary>
        public async Task<string> DownloadUpdateAsync(UpdateInfo updateInfo, IProgress<double> progress = null)
        {
            if (updateInfo == null || string.IsNullOrEmpty(updateInfo.DownloadUrl))
                throw new ArgumentNullException(nameof(updateInfo));

            var tempDir = Path.Combine(Path.GetTempPath(), "LPStudio_Update");
            Directory.CreateDirectory(tempDir);

            var fileName = updateInfo.AssetName ?? Path.GetFileName(updateInfo.DownloadUrl);
            var filePath = Path.Combine(tempDir, fileName);

            using (var client = new HttpClient())
            {
                client.Timeout = TimeSpan.FromMinutes(10);

                using (var response = await client.GetAsync(updateInfo.DownloadUrl, HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();

                    var totalBytes = response.Content.Headers.ContentLength ?? -1L;
                    var totalRead = 0L;
                    var buffer = new byte[81920];

                    using (var stream = await response.Content.ReadAsStreamAsync())
                    using (var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None))
                    {
                        int bytesRead;
                        while ((bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                        {
                            await fileStream.WriteAsync(buffer, 0, bytesRead);
                            totalRead += bytesRead;

                            if (progress != null && totalBytes > 0)
                            {
                                var percentage = (double)totalRead / totalBytes * 100;
                                progress.Report(percentage);
                            }
                        }
                    }
                }
            }

            return filePath;
        }

        #region Вложенные классы для десериализации JSON
        private class GitHubRelease
        {
            [JsonProperty("tag_name")]
            public string TagName { get; set; }

            [JsonProperty("name")]
            public string Name { get; set; }

            [JsonProperty("body")]
            public string Body { get; set; }

            [JsonProperty("prerelease")]
            public bool Prerelease { get; set; }

            [JsonProperty("published_at")]
            public DateTime PublishedAt { get; set; }

            [JsonProperty("assets")]
            public GitHubAsset[] Assets { get; set; }
        }

        private class GitHubAsset
        {
            [JsonProperty("name")]
            public string Name { get; set; }

            [JsonProperty("browser_download_url")]
            public string BrowserDownloadUrl { get; set; }

            [JsonProperty("size")]
            public long Size { get; set; }

            [JsonProperty("content_type")]
            public string ContentType { get; set; }
        }

        #endregion
    }
}
