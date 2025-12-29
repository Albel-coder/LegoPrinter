using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

namespace WindowsForms.Services
{
    /// <summary>
    /// Вспомогательные методы для работы с обновлениями
    /// </summary>
    public static class UpdateHelper
    {
        /// <summary>
        /// Получить текущую версию приложения
        /// </summary>
        public static string GetCurrentVersion()
        {
            try
            {
                var version = Assembly.GetExecutingAssembly().GetName().Version;
                return $"{version.Major}.{version.Minor}.{version.Build}";
            }
            catch
            {
                return "1.0.0";
            }
        }

        /// <summary>
        /// Очистить версию от префиксов (v1.0.0 → 1.0.0)
        /// </summary>
        public static string CleanVersion(string version)
        {
            if (string.IsNullOrEmpty(version))
                return "0.0.0";

            return version.TrimStart('v', 'V', ' ');
        }

        /// <summary>
        /// Проверить, является ли версия newerVersion новее, чем currentVersion
        /// </summary>
        public static bool IsVersionNewer(string newerVersion, string currentVersion)
        {
            try
            {
                var v1 = new Version(CleanVersion(newerVersion));
                var v2 = new Version(CleanVersion(currentVersion));
                return v1 > v2;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Проверить, является ли версия newerVersion новее или равна currentVersion
        /// </summary>
        public static bool IsVersionNewerOrEqual(string newerVersion, string currentVersion)
        {
            try
            {
                var v1 = new Version(CleanVersion(newerVersion));
                var v2 = new Version(CleanVersion(currentVersion));
                return v1 >= v2;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Получить путь к Updater
        /// </summary>
        public static string GetUpdaterPath()
        {
            var appPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            return Path.Combine(appPath, "Updater", "LegoPrinter.Updater.exe");
        }

        /// <summary>
        /// Проверить, существует ли Updater
        /// </summary>
        public static bool UpdaterExists()
        {
            var updaterPath = GetUpdaterPath();
            return File.Exists(updaterPath);
        }

        /// <summary>
        /// Запустить процесс
        /// </summary>
        public static Process StartProcess(string fileName, string arguments, bool waitForExit = false)
        {
            var processInfo = new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                UseShellExecute = true,
                CreateNoWindow = false
            };

            var process = Process.Start(processInfo);

            if (waitForExit && process != null)
            {
                process.WaitForExit();
            }

            return process;
        }
    }
}
