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

namespace WindowsForms.Services
{
    /// <summary>
    /// Сервис для проверки и установки обновлений через GitHub
    /// </summary>
    public class GitHubUpdateService
    {
        // Константы
        private const string GITHUB_API_URL = "https://api.github.com";
        private const string DEFAULT_USER_AGENT = "LegoPrinter-App-Updater";

        // Настройки
        private readonly string _owner;
        private readonly string _repo;
        private readonly string _userAgent;
        private readonly HttpClient _httpClient;

        // Кэш для хранения последней проверки
        private UpdateInfo _cachedUpdateInfo;
        private DateTime _lastCheckTime = DateTime.MinValue;
        private readonly TimeSpan _cacheDuration = TimeSpan.FromHours(1);

        /// <summary>
        /// Конструктор
        /// </summary>
        /// <param name="owner">Владелец репозитория</param>
        /// <param name="repo">Имя репозитория</param>
        public GitHubUpdateService(string owner, string repo)
        {
            _owner = owner ?? throw new ArgumentNullException(nameof(owner));
            _repo = repo ?? throw new ArgumentNullException(nameof(repo));
            _userAgent = $"{DEFAULT_USER_AGENT}/{UpdateHelper.GetCurrentVersion()}";

            // Настраиваем HttpClient
            _httpClient = new HttpClient();
            _httpClient.DefaultRequestHeaders.UserAgent.ParseAdd(_userAgent);
            _httpClient.Timeout = TimeSpan.FromSeconds(30);

            // Если нужно, можно добавить GitHub токен для увеличения лимитов
            // if (!string.IsNullOrEmpty(githubToken))
            // {
            //     _httpClient.DefaultRequestHeaders.Authorization = 
            //         new System.Net.Http.Headers.AuthenticationHeaderValue("token", githubToken);
            // }
        }

        /// <summary>
        /// Проверить наличие обновлений
        /// </summary>
        /// <param name="forceCheck">Принудительная проверка (игнорировать кэш)</param>
        /// <returns>Информация об обновлении</returns>
        public async Task<UpdateInfo> CheckForUpdatesAsync(bool forceCheck = false)
        {
            try
            {
                // Используем кэш, если не принудительная проверка
                if (!forceCheck && DateTime.Now - _lastCheckTime < _cacheDuration && _cachedUpdateInfo != null)
                {
                    return _cachedUpdateInfo;
                }

                // Получаем информацию о последнем релизе
                var latestRelease = await GetLatestReleaseAsync();

                if (latestRelease == null)
                {
                    return CreateNoUpdateInfo();
                }

                // Получаем текущую версию
                var currentVersion = UpdateHelper.GetCurrentVersion();
                var latestVersion = UpdateHelper.CleanVersion(latestRelease.TagName);

                // Проверяем, есть ли обновление
                bool hasUpdate = UpdateHelper.IsVersionNewer(latestVersion, currentVersion);

                // Ищем подходящий файл для нашей платформы
                var installerAsset = FindInstallerAsset(latestRelease.Assets);

                // Создаем информацию об обновлении
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

                // Кэшируем результат
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
        /// Получить информацию о последнем релизе
        /// </summary>
        private async Task<GitHubRelease> GetLatestReleaseAsync()
        {
            try
            {
                // Формируем URL для получения последнего релиза
                string url = $"{GITHUB_API_URL}/repos/{_owner}/{_repo}/releases/latest";

                var response = await _httpClient.GetAsync(url);

                if (!response.IsSuccessStatusCode)
                {
                    // Если не удалось получить последний релиз, пробуем получить список всех релизов
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
        /// Резервный метод: получить список всех релизов и взять первый
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

                // Ищем последний стабильный релиз
                var latestStable = releases?.FirstOrDefault(r => !r.Prerelease);
                return latestStable ?? releases?.FirstOrDefault();
            }
            catch
            {
                return null;
            }
        }

        /// <summary>
        /// Найти установщик среди файлов релиза
        /// </summary>
        private GitHubAsset FindInstallerAsset(GitHubAsset[] assets)
        {
            if (assets == null || assets.Length == 0)
                return null;

            // Ключевые слова для поиска установщика Windows
            string[] windowsKeywords = { "windows", "win", ".exe", ".msi", "setup", "installer" };
            string[] excludeKeywords = { "android", "macos", "linux", ".dmg", ".deb", ".rpm", ".apk" };

            // Ищем в порядке приоритета:

            // 1. Ищем файл с явным указанием Windows и архитектуры
            foreach (var asset in assets)
            {
                var name = asset.Name.ToLower();

                // Пропускаем файлы других платформ
                if (excludeKeywords.Any(k => name.Contains(k)))
                    continue;

                // Проверяем архитектуру (если указана)
                bool is64Bit = Environment.Is64BitProcess;
                bool hasCorrectArchitecture =
                    (is64Bit && (name.Contains("x64") || name.Contains("64") || name.Contains("x86_64"))) ||
                    (!is64Bit && (name.Contains("x86") || name.Contains("32") || name.Contains("i386")));

                // Если файл содержит ключевые слова Windows и правильную архитектуру (или архитектура не указана)
                if (windowsKeywords.Any(k => name.Contains(k)) &&
                    (hasCorrectArchitecture || !name.Contains("x64") && !name.Contains("x86")))
                {
                    return asset;
                }
            }

            // 2. Ищем любой .exe или .msi файл
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

            // 3. Ищем ZIP архив с Windows в названии
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

            // 4. Возвращаем первый файл, если ничего не нашли
            return assets.FirstOrDefault();
        }

        /// <summary>
        /// Создать информацию об отсутствии обновлений
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
        /// Запустить процесс обновления
        /// </summary>
        public void StartUpdateProcess(UpdateInfo updateInfo)
        {
            if (updateInfo == null)
                throw new ArgumentNullException(nameof(updateInfo));

            if (!updateInfo.IsAvailable)
                throw new InvalidOperationException("Обновление недоступно");

            if (string.IsNullOrEmpty(updateInfo.DownloadUrl))
                throw new InvalidOperationException("URL для скачивания не указан");

            // Проверяем существование Updater
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

                // Формируем аргументы командной строки для Updater
                var arguments = BuildUpdaterArguments(updateInfo);
                Console.WriteLine($"Аргументы Updater: {arguments}");

                // Запускаем Updater
                var updaterPath = UpdateHelper.GetUpdaterPath();
                Console.WriteLine($"Путь к Updater: {updaterPath}");
                Console.WriteLine($"Exists: {File.Exists(updaterPath)}");

                // Проверяем аргументы более подробно
                Console.WriteLine($"\nДетали аргументов:");
                Console.WriteLine($"App Exe: {Application.ExecutablePath}");
                Console.WriteLine($"App Dir: {Application.StartupPath}");
                Console.WriteLine($"Download URL: {updateInfo.DownloadUrl}");
                Console.WriteLine($"Version: {updateInfo.LatestVersion}");
                Console.WriteLine($"Asset Name: {updateInfo.AssetName}");

                // Создаем временный файл с логами
                var tempLogFile = Path.Combine(Path.GetTempPath(), $"updater_launch_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
                File.WriteAllText(tempLogFile,
                    $"Updater Path: {updaterPath}\n" +
                    $"Arguments: {arguments}\n" +
                    $"Time: {DateTime.Now}");

                Console.WriteLine($"Log file created: {tempLogFile}");

                // Запускаем Updater с детальным логированием
                var process = UpdateHelper.StartProcess(updaterPath, arguments, false);

                if (process == null)
                {
                    Console.WriteLine("Process.Start вернул null!");
                    throw new Exception("Не удалось запустить процесс Updater");
                }

                Console.WriteLine($"Updater запущен, PID: {process.Id}");
                Console.WriteLine($"Process HasExited: {process.HasExited}");

                // Даем Updater немного времени на запуск
                Thread.Sleep(1000);

                if (process.HasExited)
                {
                    Console.WriteLine($"Updater завершился с кодом: {process.ExitCode}");
                    throw new Exception($"Updater завершился сразу после запуска. Код: {process.ExitCode}");
                }

                Console.WriteLine("Updater успешно запущен, закрываю основное приложение...");

                // Закрываем текущее приложение
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
        /// Сформировать аргументы для Updater
        /// </summary>
        private string BuildUpdaterArguments(UpdateInfo updateInfo)
        {
            var args = new StringBuilder();

            // Основные параметры
            args.Append($"--app-exe \"{Application.ExecutablePath}\" ");
            args.Append($"--app-dir \"{Application.StartupPath}\" ");
            args.Append($"--download-url \"{updateInfo.DownloadUrl}\" ");
            args.Append($"--version \"{updateInfo.LatestVersion}\" ");
            args.Append($"--wait-pid {Process.GetCurrentProcess().Id} ");

            // Дополнительные параметры
            if (!string.IsNullOrEmpty(updateInfo.AssetName))
                args.Append($"--asset-name \"{updateInfo.AssetName}\" ");

            return args.ToString();
        }

        /// <summary>
        /// Скачать файл обновления напрямую (без Updater)
        /// </summary>
        public async Task<string> DownloadUpdateAsync(UpdateInfo updateInfo, IProgress<double> progress = null)
        {
            if (updateInfo == null || string.IsNullOrEmpty(updateInfo.DownloadUrl))
                throw new ArgumentNullException(nameof(updateInfo));

            var tempDir = Path.Combine(Path.GetTempPath(), "WindowsForms_Update");
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
