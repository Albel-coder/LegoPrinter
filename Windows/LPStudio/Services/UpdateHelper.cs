using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

namespace LPStudio.Services
{
    public static class UpdateHelper
    {
        /// <summary>
        /// Получает текущую версию приложения (в формате 4 чисел)
        /// </summary>
        public static string GetCurrentVersion()
        {
            try
            {
                var assembly = Assembly.GetExecutingAssembly();
                var fileVersionInfo = FileVersionInfo.GetVersionInfo(assembly.Location);

                if (!string.IsNullOrEmpty(fileVersionInfo.ProductVersion))
                {
                    return CleanVersion(fileVersionInfo.ProductVersion);
                }

                return CleanVersion(fileVersionInfo.FileVersion ?? "1.0.0.0");
            }
            catch
            {
                return "1.0.0.0";
            }
        }

        /// <summary>
        /// Очищает строку версии до формата major.minor.patch.build (4 числа)
        /// Всегда возвращает 4 числа
        /// </summary>
        public static string CleanVersion(string version)
        {
            if (string.IsNullOrWhiteSpace(version))
                return "0.0.0.0";

            version = version.Trim();

            if (version.StartsWith("v", StringComparison.OrdinalIgnoreCase))
                version = version.Substring(1);

            int dashIndex = version.IndexOfAny(new[] { '-', '+' });
            if (dashIndex > 0)
                version = version.Substring(0, dashIndex);

            var parts = version.Split(
                new[] { '.' },
                StringSplitOptions.RemoveEmptyEntries
            );

            int major = parts.Length > 0 && int.TryParse(parts[0], out var m) ? m : 0;
            int minor = parts.Length > 1 && int.TryParse(parts[1], out var n) ? n : 0;
            int patch = parts.Length > 2 && int.TryParse(parts[2], out var p) ? p : 0;
            int build = parts.Length > 3 && int.TryParse(parts[3], out var b) ? b : 0;

            return $"{major}.{minor}.{patch}.{build}";
        }


        /// <summary>
        /// Парсит версию в формате 4 чисел на 3-числовую версию и номер сборки
        /// </summary>
        public static VersionInfo ParseVersionAndBuild(string versionString)
        {
            try
            {
                var cleanVersion = CleanVersion(versionString);
                var parts = cleanVersion.Split('.');

                // Первые три числа - версия для пользователя
                var userVersion = $"{parts[0]}.{parts[1]}.{parts[2]}";

                // Четвертое число - номер сборки
                var build = int.Parse(parts[3]);

                return new VersionInfo(userVersion, build);
            }
            catch
            {
                // В случае ошибки возвращаем дефолтные значения
                return new VersionInfo("0.0.0.0", 0);
            }
        }

        /// <summary>
        /// Сравнивает версии с учетом сборок
        /// version1, version2 - в формате 3 чисел (major.minor.patch)
        /// build1, build2 - номера сборок
        /// </summary>
        public static int CompareVersionsWithBuild(
            string version1, int build1,
            string version2, int build2)
        {
            try
            {
                var v1 = new Version($"{version1}.{build1}");
                var v2 = new Version($"{version2}.{build2}");

                return v1.CompareTo(v2);
            }
            catch
            {
                return -1; // В случае ошибки считаем, что version1 старше
            }
        }

        /// <summary>
        /// Проверяет, доступна ли новая версия
        /// </summary>
        public static bool IsUpdateAvailable(string currentVersion, int currentBuild,
                                             string availableVersion, int availableBuild)
        {
            return CompareVersionsWithBuild(availableVersion, availableBuild,
                                           currentVersion, currentBuild) > 0;
        }

        /// <summary>
        /// Проверяет, является ли версия минимально требуемой
        /// </summary>
        public static bool IsMinVersionRequired(string currentVersion, int currentBuild,
                                                string minVersion, int minBuild)
        {
            return CompareVersionsWithBuild(currentVersion, currentBuild,
                                           minVersion, minBuild) >= 0;
        }

        /// <summary>
        /// Получает путь к Updater
        /// </summary>
        public static string GetUpdaterPath()
        {
            var appPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            return Path.Combine(appPath, "LegoPrinter.Updater.exe");
        }

        /// <summary>
        /// Проверяет существование Updater
        /// </summary>
        public static bool UpdaterExists()
        {
            var updaterPath = GetUpdaterPath();
            return File.Exists(updaterPath);
        }

        /// <summary>
        /// Запускает процесс
        /// </summary>
        public static Process StartProcess(string fileName, string arguments, bool waitForExit = false)
        {
            Console.WriteLine($"\n[StartProcess] Запуск процесса:");
            Console.WriteLine($"  FileName: {fileName}");
            Console.WriteLine($"  Arguments: {arguments}");
            Console.WriteLine($"  WaitForExit: {waitForExit}");

            var processInfo = new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                UseShellExecute = false,
                CreateNoWindow = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            var process = Process.Start(processInfo);

            if (process != null)
            {
                process.OutputDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Console.WriteLine($"[Updater Output]: {e.Data}");
                };

                process.ErrorDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Console.WriteLine($"[Updater Error]: {e.Data}");
                };

                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                Console.WriteLine($"  Process ID: {process.Id}");
                Console.WriteLine($"  Process Handle: {process.Handle}");
            }
            else
            {
                Console.WriteLine("  Process.Start вернул null!");
            }

            if (waitForExit && process != null)
            {
                process.WaitForExit();
            }

            return process;
        }
    }

    /// <summary>
    /// Структура для хранения версии (3 числа) и сборки
    /// </summary>
    public class VersionInfo
    {
        public string Version { get; } // 3 числа: major.minor.patch
        public int Build { get; }      // Четвертое число: build

        public VersionInfo(string version, int build)
        {
            Version = version;
            Build = build;
        }

        public void Deconstruct(out string version, out int build)
        {
            version = Version;
            build = Build;
        }

        public override string ToString()
        {
            return $"{Version}.{Build}";
        }
    }
}
