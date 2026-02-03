using CommandLine;
using SharpCompress.Archives;
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WindowsForms.Updater
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.Title = "LPStudio Updater";
            Console.WriteLine("=== LPStudio AutoUpdater ===");
            Console.WriteLine();

            try
            {
                var result = Parser.Default.ParseArguments<UpdateOptions>(args);
                await result.WithParsedAsync(RunUpdateAsync);
                result.WithNotParsed(HandleParseError);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Critical error: {ex.Message}");
                Console.WriteLine("Press any key to exit...");
                Console.ReadKey();
            }
        }

        private static async Task RunUpdateAsync(UpdateOptions options)
        {
            Console.WriteLine($"=== Запуск обновления до версии {options.Version} (build {options.Build}) ===");

            try
            {
                // 1. Проверяем контрольную сумму, если указана
                if (!string.IsNullOrEmpty(options.Checksum))
                {
                    Console.WriteLine($"Ожидаемая контрольная сумма: {options.Checksum}");
                }

                // 2. Ждем завершения основного приложения
                if (options.WaitPid > 0)
                {
                    await WaitForProcessToExit(options.WaitPid);
                }

                // 3. Создаем бекап
                Console.WriteLine("Создание резервной копии...");
                CreateBackup(options.AppDir);

                // 4. Скачиваем обновление
                Console.WriteLine($"Скачивание обновления...");
                var downloadedFile = await DownloadFileAsync(options.DownloadUrl, options.AssetName);

                // 5. Проверяем контрольную сумму
                if (!string.IsNullOrEmpty(options.Checksum))
                {
                    Console.WriteLine("Проверка контрольной суммы...");
                    if (!VerifyChecksum(downloadedFile, options.Checksum))
                    {
                        throw new Exception("Контрольная сумма не совпадает. Файл может быть поврежден.");
                    }
                    Console.WriteLine("Контрольная сумма совпадает ✓");
                }

                // 6. Обрабатываем скачанный файл
                Console.WriteLine($"Обработка файла...");
                await ProcessDownloadedFile(downloadedFile, options.AppDir, options.AssetName);

                // 7. Очистка временных файлов
                CleanUpTempFiles();

                // 8. Запуск обновленного приложения
                Console.WriteLine($"\n=== Запуск обновленного приложения ===");

                Console.WriteLine("Ожидание завершения установки...");
                await Task.Delay(10000);

                bool appLaunched = LaunchUpdatedApp(options.AppExe, options);

                if (appLaunched)
                {
                    Console.WriteLine($"\nПриложение успешно запущено!");

                    var updateMarker = Path.Combine(Path.GetTempPath(), $"LPStudio_Update_Success_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
                    File.WriteAllText(updateMarker, $"Обновление завершено {DateTime.Now}\nВерсия: {options.Version}\nСборка: {options.Build}");
                }
                else
                {
                    Console.WriteLine($"\nНе удалось запустить приложение автоматически.");
                    Console.WriteLine($"Рекомендуемые действия:");
                    Console.WriteLine($"1. Перезагрузите компьютер");
                    Console.WriteLine($"2. Запустите приложение вручную через ярлык на рабочем столе");
                }

                Console.WriteLine();
                Console.WriteLine("=== Обновление завершено ===");

                if (!options.Silent)
                {
                    Console.WriteLine("Нажмите любую клавишу для выхода...");
                    Console.ReadKey();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка во время обновления: {ex.Message}");
                Console.WriteLine("Попытка восстановления из резервной копии...");
                RestoreFromBackup(options.AppDir);

                if (!options.Silent)
                {
                    Console.WriteLine("Нажмите любую клавишу для выхода...");
                    Console.ReadKey();
                }
                throw;
            }
        }

        private static async Task WaitForProcessToExit(int pid)
        {
            Console.Write("Ожидание завершения основного приложения...");

            try
            {
                var process = Process.GetProcessById(pid);

                for (int i = 0; i < 30; i++)
                {
                    if (process.HasExited)
                    {
                        Console.WriteLine(" OK");
                        return;
                    }

                    Console.Write(".");
                    await Task.Delay(1000);
                }

                Console.WriteLine("\nПринудительное завершение...");
                process.Kill();
                await Task.Delay(2000);
            }
            catch
            {
                Console.WriteLine(" OK (уже завершено)");
            }
        }

        private static void CreateBackup(string appDir)
        {
            var backupDir = Path.Combine(Path.GetTempPath(), $"LPStudio_Backup_{DateTime.Now:yyyyMMdd_HHmmss}");
            Directory.CreateDirectory(backupDir);

            try
            {
                var files = Directory.GetFiles(appDir, "*.*", SearchOption.TopDirectoryOnly);
                foreach (var file in files)
                {
                    var fileName = Path.GetFileName(file);

                    if (fileName.Contains("Updater") ||
                        fileName.EndsWith(".tmp") ||
                        fileName.EndsWith(".log"))
                        continue;

                    var backupPath = Path.Combine(backupDir, fileName);
                    File.Copy(file, backupPath, true);
                }

                Console.WriteLine($"Резервная копия создана: {backupDir}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Предупреждение: Не удалось создать полную резервную копию: {ex.Message}");
            }
        }

        private static async Task<string> DownloadFileAsync(string url, string assetName)
        {
            var tempDir = Path.Combine(Path.GetTempPath(), "LPStudio_Update");
            Directory.CreateDirectory(tempDir);

            var fileName = assetName ?? Path.GetFileName(url);
            var filePath = Path.Combine(tempDir, fileName);

            Console.WriteLine($"URL: {url}");
            Console.WriteLine($"Файл: {filePath}");

            using (var client = new HttpClient())
            {
                client.Timeout = TimeSpan.FromMinutes(10);

                using (var response = await client.GetAsync(url, HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();

                    var totalBytes = response.Content.Headers.ContentLength ?? -1L;

                    using (var stream = await response.Content.ReadAsStreamAsync())
                    using (var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None))
                    {
                        await CopyStreamWithProgress(stream, fileStream, totalBytes);
                    }
                }
            }

            var fileInfo = new FileInfo(filePath);
            Console.WriteLine($"\nФайл скачан: {fileInfo.Length / 1024 / 1024} MB");
            return filePath;
        }

        private static async Task CopyStreamWithProgress(Stream source, Stream destination, long totalBytes)
        {
            var buffer = new byte[81920];
            long totalRead = 0;
            int read;

            var lastUpdate = DateTime.Now;

            while ((read = await source.ReadAsync(buffer)) > 0)
            {
                await destination.WriteAsync(buffer.AsMemory(0, read));
                totalRead += read;

                if (DateTime.Now - lastUpdate > TimeSpan.FromMilliseconds(500))
                {
                    ShowProgress(totalRead, totalBytes);
                    lastUpdate = DateTime.Now;
                }
            }

            ShowProgress(totalRead, totalBytes);
            Console.WriteLine();
        }

        private static void ShowProgress(long bytesRead, long totalBytes)
        {
            if (totalBytes > 0)
            {
                var percentage = (double)bytesRead / totalBytes * 100;
                var downloadedMB = bytesRead / 1024.0 / 1024.0;
                var totalMB = totalBytes / 1024.0 / 1024.0;

                Console.Write($"\rПрогресс: {percentage:F1}% ({downloadedMB:F1}/{totalMB:F1} MB)");
            }
            else
            {
                var downloadedMB = bytesRead / 1024.0 / 1024.0;
                Console.Write($"\rСкачано: {downloadedMB:F1} MB");
            }
        }

        private static bool VerifyChecksum(string filePath, string expectedHash)
        {
            try
            {
                using (var sha256 = SHA256.Create())
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

        private static async Task ProcessDownloadedFile(string filePath, string targetDir, string assetName)
        {
            var extension = Path.GetExtension(filePath).ToLower();

            switch (extension)
            {
                case ".exe":
                    await ProcessExeFile(filePath, targetDir);
                    break;

                case ".msi":
                    await ProcessMsiFile(filePath);
                    break;

                case ".zip":
                    ProcessZipFile(filePath, targetDir);
                    break;

                default:
                    File.Copy(filePath, Path.Combine(targetDir, Path.GetFileName(filePath)), true);
                    break;
            }
        }

        private static async Task ProcessExeFile(string exePath, string targetDir)
        {
            Console.WriteLine("Запуск установщика...");

            if (!File.Exists(exePath))
            {
                throw new FileNotFoundException($"Установщик не найден: {exePath}");
            }

            var arguments = new StringBuilder();
            arguments.Append("/VERYSILENT ");
            arguments.Append("/SUPPRESSMSGBOXES ");
            arguments.Append("/NORESTART ");
            arguments.Append("/CLOSEAPPLICATIONS ");
            arguments.Append("/RESTARTAPPLICATIONS ");
            arguments.Append("/MERGETASKS=\"!desktopicon,!startmenu\" ");

            if (!string.IsNullOrEmpty(targetDir))
            {
                arguments.Append($"/DIR=\"{targetDir}\" ");
            }

            var logFile = Path.Combine(Path.GetTempPath(), $"LPStudio_Update_{DateTime.Now:yyyyMMdd_HHmmss}.log");
            arguments.Append($"/LOG=\"{logFile}\"");

            Console.WriteLine($"Установщик: {Path.GetFileName(exePath)}");
            Console.WriteLine($"Аргументы: {arguments}");

            var processInfo = new ProcessStartInfo
            {
                FileName = exePath,
                Arguments = arguments.ToString(),
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                WorkingDirectory = Path.GetDirectoryName(exePath)
            };

            using (var process = new Process())
            {
                process.StartInfo = processInfo;

                var outputBuilder = new StringBuilder();
                var errorBuilder = new StringBuilder();

                process.OutputDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                    {
                        var message = $"[Installer] {e.Data}";
                        Console.WriteLine(message);
                        outputBuilder.AppendLine(message);
                    }
                };

                process.ErrorDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                    {
                        var message = $"[ERROR] {e.Data}";
                        Console.WriteLine(message);
                        errorBuilder.AppendLine(message);
                    }
                };

                Console.WriteLine("Запуск процесса установки...");
                process.Start();
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                bool exited = process.WaitForExit(600000);

                if (!exited)
                {
                    process.Kill();
                    throw new Exception("Таймаут установки (10 минут)");
                }

                Console.WriteLine($"Код выхода установщика: {process.ExitCode}");

                if (File.Exists(logFile))
                {
                    try
                    {
                        var logContent = File.ReadAllText(logFile);
                        if (logContent.Contains("Installation process succeeded"))
                        {
                            Console.WriteLine(" Установка успешна (согласно логу)");
                        }
                        else if (logContent.Contains("Installation process failed"))
                        {
                            throw new Exception("Установка не удалась согласно логу");
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Не удалось прочитать файл лога: {ex.Message}");
                    }
                }

                if (process.ExitCode != 0)
                {
                    throw new Exception($"Установщик завершился с ошибкой: {process.ExitCode}");
                }

                Console.WriteLine($" Установщик завершил работу успешно");
            }

            Console.WriteLine("Ожидание завершения файловых операций...");
            await Task.Delay(10000);

            CleanupOldUninstallers(targetDir);
        }

        private static void CleanupOldUninstallers(string targetDir)
        {
            try
            {
                Console.WriteLine("Очистка старых деинсталляторов...");

                var allFiles = Directory.GetFiles(targetDir, "unins*.*", SearchOption.TopDirectoryOnly);
                if (allFiles.Length > 1)
                {
                    var newestUninstaller = allFiles
                        .Select(f => new FileInfo(f))
                        .OrderByDescending(f => f.LastWriteTime)
                        .FirstOrDefault();

                    foreach (var file in allFiles)
                    {
                        var fileInfo = new FileInfo(file);
                        if (fileInfo.FullName != newestUninstaller?.FullName)
                        {
                            try
                            {
                                File.Delete(file);
                                Console.WriteLine($"  Удален старый деинсталлятор: {Path.GetFileName(file)}");
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"  Предупреждение: Не удалось удалить {Path.GetFileName(file)}: {ex.Message}");
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Предупреждение: Ошибка очистки деинсталляторов: {ex.Message}");
            }
        }

        private static void ProcessZipFile(string zipPath, string targetDir)
        {
            Console.WriteLine("Распаковка ZIP архива...");

            using (var archive = SharpCompress.Archives.ArchiveFactory.Open(zipPath))
            {
                foreach (var entry in archive.Entries)
                {
                    if (!entry.IsDirectory)
                    {
                        var targetPath = Path.Combine(targetDir, entry.Key);
                        var targetPathDir = Path.GetDirectoryName(targetPath);

                        if (!Directory.Exists(targetPathDir))
                        {
                            Directory.CreateDirectory(targetPathDir);
                        }

                        Console.WriteLine($"  Распаковка: {entry.Key}");
                        entry.WriteToFile(targetPath, new SharpCompress.Common.ExtractionOptions()
                        {
                            ExtractFullPath = true,
                            Overwrite = true
                        });
                    }
                }
            }
        }

        private static async Task ProcessMsiFile(string msiPath)
        {
            Console.WriteLine("Установка MSI пакета...");

            var processInfo = new ProcessStartInfo
            {
                FileName = "msiexec.exe",
                Arguments = $"/i \"{msiPath}\" /quiet /norestart",
                UseShellExecute = true,
                CreateNoWindow = true
            };

            var process = Process.Start(processInfo);
            await WaitForExitAsync(process);

            if (process.ExitCode != 0)
            {
                throw new Exception($"MSI установка не удалась (код: {process.ExitCode})");
            }
        }

        private static Task WaitForExitAsync(Process process)
        {
            var tcs = new TaskCompletionSource<bool>();
            process.EnableRaisingEvents = true;
            process.Exited += (s, e) => tcs.TrySetResult(true);

            if (process.HasExited)
                tcs.TrySetResult(true);

            return tcs.Task;
        }

        private static bool LaunchUpdatedApp(string appExePath, UpdateOptions options)
        {
            Console.WriteLine($"\n=== Попытка запуска приложения ===");

            // 1. Сначала пытаемся использовать путь из аргументов
            if (!string.IsNullOrEmpty(appExePath) && File.Exists(appExePath))
            {
                Console.WriteLine($"Используем путь из аргументов: {appExePath}");
                return TryLaunchApp(appExePath);
            }

            // 2. Если путь не существует, пробуем найти в app-dir
            if (!string.IsNullOrEmpty(options.AppDir))
            {
                var appName = Path.GetFileName(appExePath) ?? "LPStudio.exe";
                var appInAppDir = Path.Combine(options.AppDir, appName);

                if (File.Exists(appInAppDir))
                {
                    Console.WriteLine($"Используем путь из app-dir: {appInAppDir}");
                    return TryLaunchApp(appInAppDir);
                }
                else
                {
                    Console.WriteLine($"Файл не найден в app-dir: {appInAppDir}");
                }
            }

            // 3. Если все еще не нашли, используем логику поиска по возможным путям
            Console.WriteLine("Поиск приложения по стандартным путям...");

            var possiblePaths = new List<string>();
            var appNameSearch = Path.GetFileName(appExePath) ?? "LPStudio.exe";

            // Добавляем возможные пути поиска
            if (!string.IsNullOrEmpty(options.AppDir))
            {
                possiblePaths.Add(Path.Combine(options.AppDir, appNameSearch));
            }

            var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            possiblePaths.Add(Path.Combine(localAppData, "Programs", "LPStudio", appNameSearch));

            var programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            possiblePaths.Add(Path.Combine(programFiles, "LPStudio", appNameSearch));

            var programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
            possiblePaths.Add(Path.Combine(programFilesX86, "LPStudio", appNameSearch));

            // Текущая директория апдейтера
            possiblePaths.Add(Path.Combine(Directory.GetCurrentDirectory(), appNameSearch));

            // Директория, из которой был запущен апдейтер
            var updaterExe = Process.GetCurrentProcess().MainModule?.FileName;
            if (!string.IsNullOrEmpty(updaterExe))
            {
                var updaterDir = Path.GetDirectoryName(updaterExe);
                possiblePaths.Add(Path.Combine(updaterDir, appNameSearch));
            }

            // Ищем приложение
            string foundPath = null;
            foreach (var path in possiblePaths)
            {
                if (File.Exists(path))
                {
                    foundPath = path;
                    Console.WriteLine($"Найден исполняемый файл: {path}");
                    break;
                }
            }

            if (foundPath == null)
            {
                Console.WriteLine("Исполняемый файл приложения не найден в возможных путях:");
                foreach (var path in possiblePaths)
                {
                    Console.WriteLine($"  • {path}");
                }
                return false;
            }

            return TryLaunchApp(foundPath);
        }

        private static bool TryLaunchApp(string appExePath)
        {
            try
            {
                Console.WriteLine($"\nЗапуск приложения: {appExePath}");

                if (!File.Exists(appExePath))
                {
                    Console.WriteLine($"Файл не существует: {appExePath}");
                    return false;
                }

                // Подготовка информации о процессе
                var processInfo = new ProcessStartInfo
                {
                    FileName = appExePath,
                    WorkingDirectory = Path.GetDirectoryName(appExePath),
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Normal
                };

                Console.WriteLine($"Рабочая директория: {processInfo.WorkingDirectory}");
                Console.WriteLine($"Размер файла: {new FileInfo(appExePath).Length} байт");

                // Запуск
                var process = Process.Start(processInfo);

                if (process == null)
                {
                    Console.WriteLine("Не удалось создать процесс");
                    return false;
                }

                Console.WriteLine($"Приложение запущено (PID: {process.Id})");

                // Ждем немного и проверяем, не завершилось ли приложение сразу
                Thread.Sleep(3000);

                if (process.HasExited)
                {
                    Console.WriteLine($"Приложение завершилось сразу (ExitCode: {process.ExitCode})");
                    return false;
                }

                Console.WriteLine("Приложение успешно запущено и работает");
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка при запуске приложения: {ex.Message}");
                Console.WriteLine($"StackTrace: {ex.StackTrace}");
                return false;
            }
        }


        private static void RestoreFromBackup(string appDir)
        {
            var backupDirs = Directory.GetDirectories(Path.GetTempPath(), "LPStudio_Backup_*")
                .OrderByDescending(d => d)
                .ToArray();

            if (backupDirs.Length > 0)
            {
                var latestBackup = backupDirs[0];
                Console.WriteLine($"Восстановление из резервной копии: {latestBackup}");

                try
                {
                    var files = Directory.GetFiles(latestBackup, "*.*", SearchOption.TopDirectoryOnly);
                    foreach (var file in files)
                    {
                        var fileName = Path.GetFileName(file);
                        var targetPath = Path.Combine(appDir, fileName);
                        File.Copy(file, targetPath, true);
                    }

                    Console.WriteLine("Восстановление успешно завершено");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Ошибка при восстановлении: {ex.Message}");
                }
            }
        }

        private static void CleanUpTempFiles()
        {
            try
            {
                Console.WriteLine("Очистка временных файлов...");

                var currentDir = AppDomain.CurrentDomain.BaseDirectory;
                var oldFiles = Directory.GetFiles(currentDir, "*.old", SearchOption.TopDirectoryOnly);

                foreach (var oldFile in oldFiles)
                {
                    try
                    {
                        File.Delete(oldFile);
                        Console.WriteLine($"  Удален: {Path.GetFileName(oldFile)}");
                    }
                    catch
                    {
                        // Игнорируем ошибки удаления
                    }
                }

                var tempUpdateDir = Path.Combine(Path.GetTempPath(), "LPStudio_Update");
                if (Directory.Exists(tempUpdateDir))
                {
                    try
                    {
                        Directory.Delete(tempUpdateDir, true);
                        Console.WriteLine($"  Удалена временная директория: {tempUpdateDir}");
                    }
                    catch
                    {
                        // Игнорируем
                    }
                }
            }
            catch
            {
                // Игнорируем ошибки очистки
            }
        }

        private static void HandleParseError(IEnumerable<Error> errs)
        {
            Console.WriteLine("Ошибка парсинга аргументов командной строки:");
            foreach (var err in errs)
            {
                Console.WriteLine($"  {err}");
            }

            Console.WriteLine("\nНажмите любую клавишу для выхода...");
            Console.ReadKey();
        }
    }

    public class UpdateOptions
    {
        [Option("app-exe", Required = true, HelpText = "Путь к исполняемому файлу приложения")]
        public string AppExe { get; set; }

        [Option("app-dir", Required = true, HelpText = "Директория приложения")]
        public string AppDir { get; set; }

        [Option("download-url", Required = true, HelpText = "URL для скачивания обновления")]
        public string DownloadUrl { get; set; }

        [Option("asset-name", Required = false, HelpText = "Имя файла для скачивания")]
        public string AssetName { get; set; }

        [Option("version", Required = false, HelpText = "Версия обновления")]
        public string Version { get; set; }

        [Option("build", Required = false, HelpText = "Номер сборки", Default = 0)]
        public int Build { get; set; }

        [Option("checksum", Required = false, HelpText = "SHA256 контрольная сумма")]
        public string Checksum { get; set; }

        [Option("wait-pid", Required = false, HelpText = "ID процесса для ожидания", Default = 0)]
        public int WaitPid { get; set; }

        [Option("silent", Required = false, HelpText = "Тихий режим")]
        public bool Silent { get; set; }
    }
}
