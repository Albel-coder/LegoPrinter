using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;

namespace LPStudio.Services
{
    /// <summary>
    /// Helper methods for working with updates
    /// </summary>
    public static class UpdateHelper
    {
        /// <summary>
        /// Get the current version of the application
        /// </summary>
        public static string GetCurrentVersion()
        {
            try
            {
                var assembly = Assembly.GetExecutingAssembly();
                var fileVersionInfo = FileVersionInfo.GetVersionInfo(assembly.Location);

                // ProductVersion often contains the full version in human-readable format
                if (!string.IsNullOrEmpty(fileVersionInfo.ProductVersion))
                {
                    return CleanVersion(fileVersionInfo.ProductVersion);
                }

                // Fallback
                return CleanVersion(fileVersionInfo.FileVersion ?? "1.0.0");
            }
            catch
            {
                return "1.0.0.0";
            }
        }

        public static string CleanVersion(string version)
        {
            if (string.IsNullOrEmpty(version))
                return "0.0.0.0";

            // Remove prefixes and trim spaces
            version = version.Trim().TrimStart('v', 'V');

            // Remove possible suffixes like "-beta", "+build", etc.
            var dashIndex = version.IndexOf('-');
            var plusIndex = version.IndexOf('+');

            if (dashIndex > 0) version = version.Substring(0, dashIndex);
            if (plusIndex > 0) version = version.Substring(0, plusIndex);

            return version.Trim();
        }

        /// <summary>
        /// Check if newerVersion is newer than currentVersion
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
        /// Check if newerVersion is newer than or equal to currentVersion
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
        /// Get the path to Updater
        /// </summary>
        public static string GetUpdaterPath()
        {
            var appPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            Console.WriteLine($"Путь к приложению: {appPath}");

            var updaterPath = Path.Combine(appPath, "LegoPrinter.Updater.exe");
            Console.WriteLine($"Путь к Updater: {updaterPath}");
            Console.WriteLine($"Updater существует: {File.Exists(updaterPath)}");

            return updaterPath;
        }

        /// <summary>
        /// Check if Updater exists
        /// </summary>
        public static bool UpdaterExists()
        {
            var updaterPath = GetUpdaterPath();
            return File.Exists(updaterPath);
        }

        /// <summary>
        /// Start process
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
                UseShellExecute = false, // Change to false to see the console
                CreateNoWindow = false,  // Show the window
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            var process = Process.Start(processInfo);

            if (process != null)
            {
                // Read the Updater output
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
}
