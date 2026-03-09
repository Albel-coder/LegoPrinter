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
            Console.WriteLine($"=== Launching the update to version {options.Version} (build {options.Build}) ===");

            try
            {
                // We check the checksum if specified
                if (!string.IsNullOrEmpty(options.Checksum))
                {
                    Console.WriteLine($"Expected checksum: {options.Checksum}");
                }

                // Waiting for the main application to complete
                if (options.WaitPid > 0)
                {
                    await WaitForProcessToExit(options.WaitPid);
                }

                // Create a backup
                Console.WriteLine("Creating a backup copy...");
                CreateBackup(options.AppDir);

                // Downloading the update
                Console.WriteLine($"Downloading update...");
                var downloadedFile = await DownloadFileAsync(options.DownloadUrl, options.AssetName);

                // Checking the checksum
                if (!string.IsNullOrEmpty(options.Checksum))
                {
                    Console.WriteLine("Checksum verification...");
                    if (!VerifyChecksum(downloadedFile, options.Checksum))
                    {
                        throw new Exception("The checksum does not match. The file may be corrupted.");
                    }
                    Console.WriteLine("Checksum matches");
                }

                // Processing the downloaded file
                Console.WriteLine($"File processing...");
                await ProcessDownloadedFile(downloadedFile, options.AppDir, options.AssetName);

                // Cleaning temporary files
                CleanUpTempFiles();

                // Launching the updated application
                Console.WriteLine($"\n=== Launching the updated application ===");

                Console.WriteLine("Waiting for installation to complete...");
                await Task.Delay(10000);

                bool appLaunched = LaunchUpdatedApp(options.AppExe, options);

                if (appLaunched)
                {
                    Console.WriteLine($"\nThe application has been launched successfully!");

                    var updateMarker = Path.Combine(Path.GetTempPath(), $"LPStudio_Update_Success_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
                    File.WriteAllText(updateMarker, $"Update complete {DateTime.Now}\nVersion: {options.Version}\nBuild: {options.Build}");
                }
                else
                {
                    Console.WriteLine($"\nThe application failed to start automatically.");
                    Console.WriteLine($"Recommended actions:");
                    Console.WriteLine($"1. Restart your computer");
                    Console.WriteLine($"2. Launch the application manually via the desktop shortcut");
                }

                Console.WriteLine();
                Console.WriteLine("===Update Complete ===");

                if (!options.Silent)
                {
                    Console.WriteLine("Press any key to exit...");
                    Console.ReadKey();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during update: {ex.Message}");
                Console.WriteLine("Attempting to restore from backup...");
                RestoreFromBackup(options.AppDir);

                if (!options.Silent)
                {
                    Console.WriteLine("Press any key to exit...");
                    Console.ReadKey();
                }
                throw;
            }
        }

        private static async Task WaitForProcessToExit(int pid)
        {
            Console.Write("Waiting for main application to complete...");

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

                Console.WriteLine("\nForced termination...");
                process.Kill();
                await Task.Delay(2000);
            }
            catch
            {
                Console.WriteLine(" OK (already completed)");
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

                Console.WriteLine($"Backup created: {backupDir}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Warning: Failed to create full backup: {ex.Message}");
            }
        }

        private static async Task<string> DownloadFileAsync(string url, string assetName)
        {
            var tempDir = Path.Combine(Path.GetTempPath(), "LPStudio_Update");
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
            Console.WriteLine("Launching installer...");

            if (!File.Exists(exePath))
            {
                throw new FileNotFoundException($"Installer not found: {exePath}");
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

            Console.WriteLine($"Installer: {Path.GetFileName(exePath)}");
            Console.WriteLine($"Arguments: {arguments}");

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

                Console.WriteLine("Starting installation process...");
                process.Start();
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                bool exited = process.WaitForExit(600000);

                if (!exited)
                {
                    process.Kill();
                    throw new Exception("Installation timeout (10 minutes)");
                }

                Console.WriteLine($"Installer exit code: {process.ExitCode}");

                if (File.Exists(logFile))
                {
                    try
                    {
                        var logContent = File.ReadAllText(logFile);
                        if (logContent.Contains("Installation process succeeded"))
                        {
                            Console.WriteLine(" Installation successful (according to the log)");
                        }
                        else if (logContent.Contains("Installation process failed"))
                        {
                            throw new Exception("Installation failed according to the log");
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Failed to read log file: {ex.Message}");
                    }
                }

                if (process.ExitCode != 0)
                {
                    throw new Exception($"The installer failed with the following error: {process.ExitCode}");
                }

                Console.WriteLine($" The installer completed successfully.");
            }

            Console.WriteLine("Waiting for file operations to complete...");
            await Task.Delay(10000);

            CleanupOldUninstallers(targetDir);
        }

        private static void CleanupOldUninstallers(string targetDir)
        {
            try
            {
                Console.WriteLine("Cleaning up old uninstallers...");

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
                                Console.WriteLine($"  Removed old uninstaller: {Path.GetFileName(file)}");
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"  Warning: Failed to delete {Path.GetFileName(file)}: {ex.Message}");
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Warning: Error cleaning uninstallers: {ex.Message}");
            }
        }

        private static void ProcessZipFile(string zipPath, string targetDir)
        {
            Console.WriteLine("Unpacking the ZIP archive...");

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

                        Console.WriteLine($"  Unboxing: {entry.Key}");
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

        private static bool LaunchUpdatedApp(string appExePath, UpdateOptions options)
        {
            Console.WriteLine($"\n=== Trying to launch the application ===");

            // First we try to use the path from the arguments
            if (!string.IsNullOrEmpty(appExePath) && File.Exists(appExePath))
            {
                Console.WriteLine($"We use the path from the arguments: {appExePath}");
                return TryLaunchApp(appExePath);
            }

            // If the path doesn't exist, try to find it in app-dir
            if (!string.IsNullOrEmpty(options.AppDir))
            {
                var appName = Path.GetFileName(appExePath) ?? "LPStudio.exe";
                var appInAppDir = Path.Combine(options.AppDir, appName);

                if (File.Exists(appInAppDir))
                {
                    Console.WriteLine($"We use the path from app-dir: {appInAppDir}");
                    return TryLaunchApp(appInAppDir);
                }
                else
                {
                    Console.WriteLine($"File not found in app-dir: {appInAppDir}");
                }
            }

            // If you still haven't found it, use the search logic for possible paths
            Console.WriteLine("Searching for an application using standard paths...");

            var possiblePaths = new List<string>();
            var appNameSearch = Path.GetFileName(appExePath) ?? "LPStudio.exe";

            // Add possible search paths
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

            // Current updater directory
            possiblePaths.Add(Path.Combine(Directory.GetCurrentDirectory(), appNameSearch));

            // The directory from which the updater was launched
            var updaterExe = Process.GetCurrentProcess().MainModule?.FileName;
            if (!string.IsNullOrEmpty(updaterExe))
            {
                var updaterDir = Path.GetDirectoryName(updaterExe);
                possiblePaths.Add(Path.Combine(updaterDir, appNameSearch));
            }

            // Looking for an application
            string foundPath = null;
            foreach (var path in possiblePaths)
            {
                if (File.Exists(path))
                {
                    foundPath = path;
                    Console.WriteLine($"Executable file found: {path}");
                    break;
                }
            }

            if (foundPath == null)
            {
                Console.WriteLine("The application executable file was not found in the possible paths:");
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
                Console.WriteLine($"\nLaunching the application: {appExePath}");

                if (!File.Exists(appExePath))
                {
                    Console.WriteLine($"File does not exist: {appExePath}");
                    return false;
                }

                // Preparing process information
                var processInfo = new ProcessStartInfo
                {
                    FileName = appExePath,
                    WorkingDirectory = Path.GetDirectoryName(appExePath),
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Normal
                };

                Console.WriteLine($"Working directory: {processInfo.WorkingDirectory}");
                Console.WriteLine($"File size: {new FileInfo(appExePath).Length} byte");

                // Launch
                var process = Process.Start(processInfo);

                if (process == null)
                {
                    Console.WriteLine("Failed to create process");
                    return false;
                }

                Console.WriteLine($"Application started (PID: {process.Id})");

                // Wait a bit and check if the application terminates immediately.
                Thread.Sleep(3000);

                if (process.HasExited)
                {
                    Console.WriteLine($"The application terminated immediately (ExitCode: {process.ExitCode})");
                    return false;
                }

                Console.WriteLine("The application has been successfully launched and is working.");
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error starting application: {ex.Message}");
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
                Console.WriteLine($"Restoring from a backup: {latestBackup}");

                try
                {
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
                    Console.WriteLine($"Error while restoring: {ex.Message}");
                }
            }
        }

        private static void CleanUpTempFiles()
        {
            try
            {
                Console.WriteLine("Cleaning temporary files...");

                var currentDir = AppDomain.CurrentDomain.BaseDirectory;
                var oldFiles = Directory.GetFiles(currentDir, "*.old", SearchOption.TopDirectoryOnly);

                foreach (var oldFile in oldFiles)
                {
                    try
                    {
                        File.Delete(oldFile);
                        Console.WriteLine($"  Deleted: {Path.GetFileName(oldFile)}");
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }

                var tempUpdateDir = Path.Combine(Path.GetTempPath(), "LPStudio_Update");
                if (Directory.Exists(tempUpdateDir))
                {
                    try
                    {
                        Directory.Delete(tempUpdateDir, true);
                        Console.WriteLine($"  Temporary directory removed: {tempUpdateDir}");
                    }
                    catch
                    {
                        // Ignore
                    }
                }
            }
            catch
            {
                // Ignore cleanup errors
            }
        }

        private static void HandleParseError(IEnumerable<Error> errs)
        {
            Console.WriteLine("Error parsing command line arguments:");
            foreach (var err in errs)
            {
                Console.WriteLine($"  {err}");
            }

            Console.WriteLine("\nPress any key to exit...");
            Console.ReadKey();
        }
    }

    public class UpdateOptions
    {
        [Option("app-exe", Required = true, HelpText = "Path to the application executable file")]
        public string AppExe { get; set; }

        [Option("app-dir", Required = true, HelpText = "Application directory")]
        public string AppDir { get; set; }

        [Option("download-url", Required = true, HelpText = "URL for downloading the update")]
        public string DownloadUrl { get; set; }

        [Option("asset-name", Required = false, HelpText = "File name for download")]
        public string AssetName { get; set; }

        [Option("version", Required = false, HelpText = "Update version")]
        public string Version { get; set; }

        [Option("build", Required = false, HelpText = "Build number", Default = 0)]
        public int Build { get; set; }

        [Option("checksum", Required = false, HelpText = "SHA256 checksum")]
        public string Checksum { get; set; }

        [Option("wait-pid", Required = false, HelpText = "Process ID to wait", Default = 0)]
        public int WaitPid { get; set; }

        [Option("silent", Required = false, HelpText = "Quiet mode")]
        public bool Silent { get; set; }
    }
}
