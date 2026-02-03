using Newtonsoft.Json;
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace LPStudio.Services
{
    public class GitHubUpdateService
    {
        private const string DEFAULT_MANIFEST_URL = "https://github.com/{owner}/{repo}/releases/latest/download/update.json";
        private const string USER_AGENT = "LPStudio-Updater";

        private readonly string _owner;
        private readonly string _repo;
        private readonly string _manifestUrl;
        private readonly HttpClient _httpClient;
        private readonly string _currentPlatform;

        private UpdateInfo _cachedUpdateInfo;
        private DateTime _lastCheckTime = DateTime.MinValue;
        private readonly TimeSpan _cacheDuration = TimeSpan.FromMinutes(30);

        public GitHubUpdateService(string owner, string repo, string manifestUrl = null)
        {
            _owner = owner ?? throw new ArgumentNullException(nameof(owner));
            _repo = repo ?? throw new ArgumentNullException(nameof(repo));
            _manifestUrl = manifestUrl ?? DEFAULT_MANIFEST_URL.Replace("{owner}", owner).Replace("{repo}", repo);

            _currentPlatform = GetCurrentPlatform();

            _httpClient = new HttpClient();
            _httpClient.DefaultRequestHeaders.UserAgent.ParseAdd($"{USER_AGENT}/{UpdateHelper.GetCurrentVersion()}");
            _httpClient.Timeout = TimeSpan.FromSeconds(30);
        }

        public async Task<UpdateInfo> CheckForUpdatesAsync(bool forceCheck = false)
        {
            try
            {
                if (!forceCheck && DateTime.Now - _lastCheckTime < _cacheDuration && _cachedUpdateInfo != null)
                {
                    return _cachedUpdateInfo;
                }

                var manifest = await LoadUpdateManifestAsync();
                if (manifest == null)
                {
                    return CreateNoUpdateInfo();
                }

                if (!manifest.Platforms.TryGetValue(_currentPlatform, out var platformInfo))
                {
                    Debug.WriteLine($"[UpdateService] Платформа '{_currentPlatform}' не найдена в манифесте");
                    return CreateNoUpdateInfo();
                }

                // Получаем текущую версию и сборку
                var (currentVersion, currentBuild) = UpdateHelper.ParseVersionAndBuild(
                    UpdateHelper.GetCurrentVersion()
                );

                // Проверяем обновление
                var checkResult = platformInfo.CheckForUpdate(currentVersion, currentBuild);

                var updateInfo = new UpdateInfo
                {
                    IsAvailable = checkResult.IsUpdateAvailable,
                    IsRequired = checkResult.IsRequired,
                    IsDeprecated = checkResult.IsDeprecated,
                    IsCompatible = checkResult.IsCompatible,
                    IsCritical = checkResult.IsCritical,

                    CurrentVersion = currentVersion,
                    CurrentBuild = currentBuild,
                    LatestVersion = platformInfo.AvailableVersion,
                    LatestBuild = platformInfo.Build,
                    MinRequiredVersion = platformInfo.MinVersion,

                    DownloadUrl = platformInfo.Url,
                    ReleaseNotes = platformInfo.Changelog,
                    GeneralReleaseNotes = manifest.ReleaseNotes,
                    ProductName = manifest.ProductName,
                    ProductVersion = manifest.ProductVersion,
                    PublishedAt = platformInfo.ReleaseDate,
                    InstallerType = platformInfo.InstallerType,
                    FileSize = platformInfo.FileSize,
                    Checksum = platformInfo.Sha256,
                    Signature = platformInfo.Signature,
                    ChangelogUrl = manifest.ChangelogUrl,
                    AssetName = Path.GetFileName(platformInfo.Url)
                };

                _cachedUpdateInfo = updateInfo;
                _lastCheckTime = DateTime.Now;

                return updateInfo;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] Ошибка проверки обновлений: {ex.Message}");
                return CreateNoUpdateInfo();
            }
        }

        private async Task<UpdateManifest> LoadUpdateManifestAsync()
        {
            try
            {
                var json = await _httpClient.GetStringAsync(_manifestUrl);
                return JsonConvert.DeserializeObject<UpdateManifest>(json);
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[UpdateService] Ошибка загрузки манифеста: {ex.Message}");
                return null;
            }
        }

        private string GetCurrentPlatform()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                return "windows";
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                return "linux";
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
                return "macos";
            else
                return "unknown";
        }

        private UpdateInfo CreateNoUpdateInfo()
        {
            var versionInfo = UpdateHelper.ParseVersionAndBuild(UpdateHelper.GetCurrentVersion());

            return new UpdateInfo
            {
                IsAvailable = false,
                IsRequired = false,
                IsCompatible = true,
                CurrentVersion = versionInfo.Version,
                CurrentBuild = versionInfo.Build,
                LatestVersion = versionInfo.Version,
                LatestBuild = versionInfo.Build
            };
        }

        public void StartUpdateProcess(UpdateInfo updateInfo)
        {
            if (updateInfo == null)
                throw new ArgumentNullException(nameof(updateInfo));

            if (!updateInfo.IsAvailable)
                throw new InvalidOperationException("Обновление недоступно");

            if (string.IsNullOrEmpty(updateInfo.DownloadUrl))
                throw new InvalidOperationException("URL для скачивания не указан");

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

                // Получаем текущий исполняемый файл и директорию
                var currentExe = Process.GetCurrentProcess().MainModule.FileName;
                var currentDir = Path.GetDirectoryName(currentExe);

                // Собираем аргументы с точными путями
                var arguments = BuildUpdaterArguments(updateInfo, currentExe, currentDir);
                Console.WriteLine($"Аргументы Updater: {arguments}");

                var updaterPath = UpdateHelper.GetUpdaterPath();
                Console.WriteLine($"Путь к Updater: {updaterPath}");
                Console.WriteLine($"Exists: {File.Exists(updaterPath)}");

                var process = UpdateHelper.StartProcess(updaterPath, arguments, false);

                if (process == null)
                {
                    Console.WriteLine("Process.Start вернул null!");
                    throw new Exception("Не удалось запустить процесс Updater");
                }

                Console.WriteLine($"Updater запущен, PID: {process.Id}");

                System.Threading.Thread.Sleep(1000);

                if (process.HasExited)
                {
                    Console.WriteLine($"Updater завершился с кодом: {process.ExitCode}");
                    throw new Exception($"Updater завершился сразу после запуска. Код: {process.ExitCode}");
                }

                Console.WriteLine("Updater успешно запущен, закрываю основное приложение...");

                Application.Exit();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка запуска Updater: {ex.Message}");
                MessageBox.Show(
                    $"Не удалось запустить процесс обновления: {ex.Message}",
                    "Ошибка",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private string BuildUpdaterArguments(UpdateInfo updateInfo, string currentExe, string currentDir)
        {
            var args = new StringBuilder();

            // Обязательные параметры
            args.Append($"--app-exe \"{currentExe}\" ");
            args.Append($"--app-dir \"{currentDir}\" ");
            args.Append($"--download-url \"{updateInfo.DownloadUrl}\" ");

            if (!string.IsNullOrEmpty(updateInfo.AssetName))
                args.Append($"--asset-name \"{updateInfo.AssetName}\" ");

            args.Append($"--version \"{updateInfo.CurrentVersion}\" ");
            args.Append($"--build {updateInfo.CurrentBuild} ");

            if (!string.IsNullOrEmpty(updateInfo.Checksum))
                args.Append($"--checksum \"{updateInfo.Checksum}\" ");

            args.Append($"--wait-pid {Process.GetCurrentProcess().Id} ");

            //if (updateInfo.Silent)
            //    args.Append("--silent ");

            return args.ToString().Trim();
        }


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

            if (!string.IsNullOrEmpty(updateInfo.Checksum))
            {
                if (!VerifyChecksum(filePath, updateInfo.Checksum))
                {
                    File.Delete(filePath);
                    throw new Exception("Контрольная сумма файла не совпадает");
                }
            }

            return filePath;
        }

        public bool VerifyChecksum(string filePath, string expectedHash)
        {
            if (string.IsNullOrEmpty(expectedHash))
                return true;

            try
            {
                using (var sha256 = System.Security.Cryptography.SHA256.Create())
                using (var stream = File.OpenRead(filePath))
                {
                    var hashBytes = sha256.ComputeHash(stream);
                    var actualHash = BitConverter.ToString(hashBytes).Replace("-", "").ToLower();
                    var cleanExpectedHash = expectedHash.ToLower().Replace("sha256:", "");
                    return actualHash == cleanExpectedHash;
                }
            }
            catch
            {
                return false;
            }
        }
    }
}
