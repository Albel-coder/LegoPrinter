using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using CommandLine;
using SharpCompress.Archives;
using SharpCompress.Common;

namespace WindowsForms.Updater
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.Title = "WindowsForms Updater";
            Console.WriteLine("=== WindowsForms AutoUpdater ===");
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
            Console.WriteLine($"Starting update to version {options.Version}");
            Console.WriteLine();

            try
            {
                // 1. Wait for main app to close
                if (options.WaitPid > 0)
                {
                    await WaitForProcessToExit(options.WaitPid);
                }

                // 2. Create backup
                Console.WriteLine("Creating backup...");
                CreateBackup(options.AppDir);

                // 3. Download update
                Console.WriteLine($"Downloading update...");
                var downloadedFile = await DownloadFileAsync(options.DownloadUrl, options.AssetName);

                // 4. Process downloaded file
                Console.WriteLine($"Processing file...");
                await ProcessDownloadedFile(downloadedFile, options.AppDir, options.AssetName);

                // 5. Launch updated application
                Console.WriteLine($"Launching updated application...");
                LaunchUpdatedApp(options.AppExe);

                Console.WriteLine();
                Console.WriteLine("Update completed successfully!");
                Console.WriteLine("Press any key to exit...");
                Console.ReadKey();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during update: {ex.Message}");
                Console.WriteLine("Attempting to restore from backup...");
                RestoreFromBackup(options.AppDir);
                Console.WriteLine("Press any key to exit...");
                Console.ReadKey();
            }
        }

        private static async Task WaitForProcessToExit(int pid)
        {
            Console.Write("Waiting for main application to close...");

            try
            {
                var process = Process.GetProcessById(pid);

                for (int i = 0; i < 30; i++) // Wait max 30 seconds
                {
                    if (process.HasExited)
                    {
                        Console.WriteLine(" OK");
                        return;
                    }

                    Console.Write(".");
                    await Task.Delay(1000);
                }

                // If still running, try to kill
                Console.WriteLine("\nForcing termination...");
                process.Kill();
                process.WaitForExit(5000);
            }
            catch
            {
                // Process already closed
                Console.WriteLine(" OK (already closed)");
            }
        }

        private static void CreateBackup(string appDir)
        {
            var backupDir = Path.Combine(Path.GetTempPath(), $"WindowsForms_Backup_{DateTime.Now:yyyyMMdd_HHmmss}");
            Directory.CreateDirectory(backupDir);

            try
            {
                // Copy all files except temporary ones
                var files = Directory.GetFiles(appDir, "*.*", SearchOption.TopDirectoryOnly);
                foreach (var file in files)
                {
                    var fileName = Path.GetFileName(file);
                    var backupPath = Path.Combine(backupDir, fileName);

                    // Skip updater itself and temp files
                    if (fileName.Contains("Updater") ||
                        fileName.EndsWith(".tmp") ||
                        fileName.EndsWith(".log"))
                        continue;

                    File.Copy(file, backupPath, true);
                }

                Console.WriteLine($"Backup created: {backupDir}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Warning: Could not create full backup: {ex.Message}");
            }
        }

        private static async Task<string> DownloadFileAsync(string url, string assetName)
        {
            var tempDir = Path.Combine(Path.GetTempPath(), "WindowsForms_Update");
            Directory.CreateDirectory(tempDir);

            var fileName = assetName ?? Path.GetFileName(url);
            var filePath = Path.Combine(tempDir, fileName);

            Console.WriteLine($"URL: {url}");
            Console.WriteLine($"File: {filePath}");

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
            Console.WriteLine($"\nFile downloaded: {fileInfo.Length / 1024 / 1024} MB");
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

                Console.Write($"\rProgress: {percentage:F1}% ({downloadedMB:F1}/{totalMB:F1} MB)");
            }
            else
            {
                var downloadedMB = bytesRead / 1024.0 / 1024.0;
                Console.Write($"\rDownloaded: {downloadedMB:F1} MB");
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

                case ".zip":
                    ProcessZipFile(filePath, targetDir);
                    break;

                case ".msi":
                    await ProcessMsiFile(filePath);
                    break;

                default:
                    // Just copy the file
                    File.Copy(filePath, Path.Combine(targetDir, Path.GetFileName(filePath)), true);
                    break;
            }
        }

        private static async Task ProcessExeFile(string exePath, string targetDir)
        {
            Console.WriteLine("Running installer...");

            // Parameters for Inno Setup
            var arguments = $"/VERYSILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS=0";

            if (!string.IsNullOrEmpty(targetDir))
            {
                arguments += $" /DIR=\"{targetDir}\"";
            }

            var processInfo = new ProcessStartInfo
            {
                FileName = exePath,
                Arguments = arguments,
                UseShellExecute = true,
                CreateNoWindow = true
            };

            var process = Process.Start(processInfo);
            await WaitForExitAsync(process);

            if (process.ExitCode != 0)
            {
                throw new Exception($"Installer failed (code: {process.ExitCode})");
            }
        }

        private static void ProcessZipFile(string zipPath, string targetDir)
        {
            Console.WriteLine("Extracting ZIP archive...");

            using (var archive = ArchiveFactory.Open(zipPath))
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

                        Console.WriteLine($"  Extracting: {entry.Key}");
                        entry.WriteToFile(targetPath, new ExtractionOptions()
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
            Console.WriteLine("Installing MSI package...");

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
                throw new Exception($"MSI installation failed (code: {process.ExitCode})");
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

        private static void LaunchUpdatedApp(string appExePath)
        {
            if (File.Exists(appExePath))
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = appExePath,
                    WorkingDirectory = Path.GetDirectoryName(appExePath),
                    UseShellExecute = true
                });
            }
        }

        private static void RestoreFromBackup(string appDir)
        {
            var backupDirs = Directory.GetDirectories(Path.GetTempPath(), "WindowsForms_Backup_*")
                .OrderByDescending(d => d)
                .ToArray();

            if (backupDirs.Length > 0)
            {
                var latestBackup = backupDirs[0];
                Console.WriteLine($"Restoring from backup: {latestBackup}");

                try
                {
                    // Restore files from backup
                    var files = Directory.GetFiles(latestBackup, "*.*", SearchOption.TopDirectoryOnly);
                    foreach (var file in files)
                    {
                        var fileName = Path.GetFileName(file);
                        var targetPath = Path.Combine(appDir, fileName);

                        File.Copy(file, targetPath, true);
                    }

                    Console.WriteLine("Restore completed successfully");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error during restore: {ex.Message}");
                }
            }
        }

        private static void HandleParseError(IEnumerable<Error> errs)
        {
            Console.WriteLine("Error parsing command line arguments:");
            foreach (var err in errs)
            {
                Console.WriteLine($"  {err}");
            }

            Console.WriteLine("\nUsage example:");
            Console.WriteLine("WindowsForms.Updater.exe --app-exe \"C:\\Program Files\\WindowsForms\\WindowsForms.exe\" --app-dir \"C:\\Program Files\\WindowsForms\" --download-url \"https://github.com/user/repo/releases/download/v1.0.0/setup.exe\" --version 1.0.0 --wait-pid 1234");
            Console.WriteLine("\nPress any key to exit...");
            Console.ReadKey();
        }
    }

    public class UpdateOptions
    {
        [Option("app-exe", Required = true, HelpText = "Path to application executable")]
        public string AppExe { get; set; }

        [Option("app-dir", Required = true, HelpText = "Application directory")]
        public string AppDir { get; set; }

        [Option("download-url", Required = true, HelpText = "URL to download update")]
        public string DownloadUrl { get; set; }

        [Option("asset-name", Required = false, HelpText = "Name of the file to download")]
        public string AssetName { get; set; }

        [Option("version", Required = false, HelpText = "Update version")]
        public string Version { get; set; }

        [Option("wait-pid", Required = false, HelpText = "Process ID to wait for", Default = 0)]
        public int WaitPid { get; set; }

        [Option("silent", Required = false, HelpText = "Silent mode")]
        public bool Silent { get; set; }
    }
}
