using CommandLine;
using SharpCompress.Archives;
using SharpCompress.Common;
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
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
            Console.WriteLine($"=== Launching the update to version {options.Version} ===");
            Console.WriteLine($"AppExe from parameters: {options.AppExe}");
            Console.WriteLine($"AppDir from parameters: {options.AppDir}");
            Console.WriteLine();

            // Log the existence of the directory
            if (Directory.Exists(options.AppDir))
            {
                Console.WriteLine($"The directory exists: {options.AppDir}");
                var files = Directory.GetFiles(options.AppDir, "*.exe");
                Console.WriteLine($"Found {files.Length} EXE files:");
                foreach (var file in files)
                {
                    Console.WriteLine($"  - {Path.GetFileName(file)}");
                }
            }
            else
            {
                Console.WriteLine($"Directory does not exist: {options.AppDir}");
            }

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

                // 5. Clean up temporary files
                CleanUpTempFiles();

                // 6. Launch the updated application
                Console.WriteLine($"\n=== Launching the updated application ===");

                // Increase the wait time after installation
                Console.WriteLine("Waiting for installation to complete...");
                await Task.Delay(15000); // Increase to 15 seconds

                // Launch with improved search logic
                bool appLaunched = LaunchUpdatedApp(options.AppExe);

                if (appLaunched)
                {
                    Console.WriteLine($"\nThe application has been launched successfully!");

                    // Create a marker file about a successful update
                    var updateMarker = Path.Combine(Path.GetTempPath(), $"LPStudio_Update_Success_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
                    File.WriteAllText(updateMarker, $"Update completed at {DateTime.Now}\nVersion: {options.Version}");
                }
                else
                {
                    Console.WriteLine($"\nThe application failed to start automatically.");
                    Console.WriteLine($"Recommended actions:");
                    Console.WriteLine($"1. Restart your computer");
                    Console.WriteLine($"2. Launch the application manually via the desktop shortcut");
                    Console.WriteLine($"3. Or via the Start menu -> LPStudio");

                    // Show possible paths
                    Console.WriteLine($"\nLook for the application in the following folders:");
                    Console.WriteLine($"- {Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "LPStudio")}");
                    Console.WriteLine($"- {Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "LPStudio")}");
                    Console.WriteLine($"- {Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "LPStudio")}");
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
                await Task.Delay(2000); // Give it time to complete
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

            // Check that the installer exists
            if (!File.Exists(exePath))
            {
                throw new FileNotFoundException($"Installer not found: {exePath}");
            }

            // Check the installer's AppId
            Console.WriteLine("Checking installer AppId...");
            string foundAppId = ExtractAppIdFromInstaller(exePath);
            string expectedAppId = "{6A00D414-87C7-4422-A25A-EAE68F7E4B19}";

            if (foundAppId != null)
            {
                if (foundAppId == expectedAppId)
                {
                    Console.WriteLine($" Installer has correct AppId: {foundAppId}");
                }
                else
                {
                    Console.WriteLine($" WARNING: Installer has different AppId!");
                    Console.WriteLine($"  Expected: {expectedAppId}");
                    Console.WriteLine($"  Found: {foundAppId}");
                    Console.WriteLine("  This may cause update issues!");

                    // But we continue anyway - maybe this is the first installation
                }
            }
            else
            {
                Console.WriteLine("Could not extract AppId from installer");
            }

            // Prepare parameters for Inno Setup
            Console.WriteLine("Preparing installer arguments...");

            var arguments = new StringBuilder();
            arguments.Append("/VERYSILENT ");        // Completely silent mode
            arguments.Append("/SUPPRESSMSGBOXES ");  // Don't show messages
            arguments.Append("/NORESTART ");         // Do not restart the computer
            arguments.Append("/CLOSEAPPLICATIONS "); // Close running applications

            // For an update, it is important to indicate that this is a reinstallation
            arguments.Append("/RESTARTAPPLICATIONS ");

            // Disable shortcut creation during updates (to avoid duplicates)
            arguments.Append("/MERGETASKS=\"!desktopicon,!startmenu\" ");

            // Specify the installation directory
            if (!string.IsNullOrEmpty(targetDir))
            {
                arguments.Append($"/DIR=\"{targetDir}\" ");
            }

            // Logging for debugging
            var logFile = Path.Combine(Path.GetTempPath(), $"LPStudio_Update_{DateTime.Now:yyyyMMdd_HHmmss}.log");
            arguments.Append($"/LOG=\"{logFile}\"");

            Console.WriteLine($"Installer: {Path.GetFileName(exePath)}");
            Console.WriteLine($"Arguments: {arguments}");
            Console.WriteLine($"Log file: {logFile}");

            // Run the installer
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

                // Collectors for output
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

                Console.WriteLine("Starting installer process...");
                process.Start();
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                // Wait for completion with a timeout (10 minutes)
                bool exited = process.WaitForExit(600000);

                if (!exited)
                {
                    process.Kill();
                    throw new Exception("Installer timeout (10 minutes)");
                }
                Console.WriteLine($"Installer exit code: {process.ExitCode}");

                // Analyze the log file
                if (File.Exists(logFile))
                {
                    try
                    {
                        var logContent = File.ReadAllText(logFile);
                        Console.WriteLine("=== Installer Log (last 20 lines) ===");

                        var lines = logContent.Split('\n');
                        var start = Math.Max(0, lines.Length - 20);
                        for (int i = start; i < lines.Length; i++)
                        {
                            if (!string.IsNullOrWhiteSpace(lines[i]))
                                Console.WriteLine($"  {lines[i].Trim()}");
                        }
                        Console.WriteLine("=== End Log ===");

                        // Check the success of the installation using the log
                        if (logContent.Contains("Installation process succeeded"))
                        {
                            Console.WriteLine(" Installation succeeded (from log)");
                        }
                        else if (logContent.Contains("Installation process failed"))
                        {
                            throw new Exception("Installation failed according to log");
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Could not read log file: {ex.Message}");
                    }
                }

                if (process.ExitCode != 0)
                {
                    throw new Exception($"Installer failed with exit code: {process.ExitCode}");
                }

                Console.WriteLine($" Installer completed successfully");
            }

            // Give time for all file operations to complete
            Console.WriteLine("Waiting for file operations to complete...");
            await Task.Delay(10000);

            // Clean up old uninstallers
            CleanupOldUninstallers(targetDir);
        }

        private static string ExtractAppIdFromInstaller(string exePath)
        {
            try
            {
                // Read the binary file and look for AppId
                var bytes = File.ReadAllBytes(exePath);
                var content = System.Text.Encoding.ASCII.GetString(bytes);

                // Search for AppId={{
                var startIndex = content.IndexOf("AppId={{");
                if (startIndex >= 0)
                {
                    startIndex += 8; // Skipping "AppId={{"
                    var endIndex = content.IndexOf("}", startIndex);
                    if (endIndex > startIndex)
                    {
                        var appId = content.Substring(startIndex, endIndex - startIndex);
                        return appId;
                    }
                }
            }
            catch
            {
                // It is not critical if the extraction failed
            }

            return null;
        }

        private static void CleanupOldUninstallers(string targetDir)
        {
            try
            {
                Console.WriteLine("Cleaning up old uninstallers...");

                var allFiles = Directory.GetFiles(targetDir, "unins*.*", SearchOption.TopDirectoryOnly);
                if (allFiles.Length > 1)
                {
                    // Find the newest file by modification date
                    var newestUninstaller = allFiles
                        .Select(f => new FileInfo(f))
                        .OrderByDescending(f => f.LastWriteTime)
                        .FirstOrDefault();

                    // Delete all old ones
                    foreach (var file in allFiles)
                    {
                        var fileInfo = new FileInfo(file);
                        if (fileInfo.FullName != newestUninstaller?.FullName)
                        {
                            try
                            {
                                File.Delete(file);
                                Console.WriteLine($"  Deleted old uninstaller: {Path.GetFileName(file)}");
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"  Warning: Could not delete {Path.GetFileName(file)}: {ex.Message}");
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

        private static bool LaunchUpdatedApp(string appExePath)
        {
            Console.WriteLine($"=== Launching the updated application ===");

            // Give more time for all installation operations to complete
            Console.WriteLine("Waiting for all installation operations to complete...");
            Thread.Sleep(15000); // Increase to 15 seconds

            // List of priority paths to search
            var possiblePaths = new List<string>();

            // 1. The path that came in the arguments (original)
            if (!string.IsNullOrEmpty(appExePath))
            {
                possiblePaths.Add(appExePath);
                Console.WriteLine($"Added path from arguments: {appExePath}");
            }

            // 2. Standard Inno Setup installation paths
            var appName = "LPStudio.exe";

            // Path to set the current user
            var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            possiblePaths.Add(Path.Combine(localAppData, "Programs", "LPStudio", appName));

            // Installation path for all users (Program Files)
            var programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            possiblePaths.Add(Path.Combine(programFiles, "LPStudio", appName));

            // x86 path for 32-bit systems
            var programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
            possiblePaths.Add(Path.Combine(programFilesX86, "LPStudio", appName));

            // 3. Current directory and parent directories
            var currentDir = Directory.GetCurrentDirectory();
            possiblePaths.Add(Path.Combine(currentDir, appName));

            var parentDir = Directory.GetParent(currentDir)?.FullName;
            if (!string.IsNullOrEmpty(parentDir))
            {
                possiblePaths.Add(Path.Combine(parentDir, appName));
            }

            // Looking for a file
            string foundPath = null;
            foreach (var path in possiblePaths)
            {
                if (File.Exists(path))
                {
                    foundPath = path;
                    Console.WriteLine($"Executable file found: {path}");
                    break;
                }
                else
                {
                    Console.WriteLine($"File not found: {path}");
                }
            }

            if (foundPath == null)
            {
                Console.WriteLine("The application executable file could not be found.");

                // Let's try to find it using disk search
                Console.WriteLine("Trying to search drive C:");
                try
                {
                    var drives = DriveInfo.GetDrives();
                    foreach (var drive in drives)
                    {
                        if (drive.IsReady && drive.Name.StartsWith("C:"))
                        {
                            Console.WriteLine($"Search on disk {drive.Name}...");
                            var foundFiles = Directory.GetFiles(drive.RootDirectory.FullName, appName, SearchOption.AllDirectories)
                                .Where(f => f.Contains("LPStudio"))
                                .Take(5)
                                .ToList();

                            foreach (var file in foundFiles)
                            {
                                Console.WriteLine($"Found: {file}");
                                if (TryLaunchApp(file))
                                {
                                    return true;
                                }
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Search error: {ex.Message}");
                }

                return false;
            }

            // Launch the application
            return TryLaunchApp(foundPath);
        }

        private static bool TryLaunchApp(string appExePath)
        {
            Console.WriteLine($"\n=== Trying to launch the application ===");
            Console.WriteLine($"Path: {appExePath}");

            // Check the existence of the file
            if (!File.Exists(appExePath))
            {
                Console.WriteLine($"File does not exist: {appExePath}");
                return false;
            }

            try
            {
                Console.WriteLine($"File size: {new FileInfo(appExePath).Length} bytes");

                // Let's try different launch methods

                // Method 1: Using Process.Start with UseShellExecute = true
                Console.WriteLine("\nMethod 1: Launch via ShellExecute");
                try
                {
                    var process1 = new Process();
                    process1.StartInfo.FileName = appExePath;
                    process1.StartInfo.UseShellExecute = true;
                    process1.StartInfo.WorkingDirectory = Path.GetDirectoryName(appExePath);
                    process1.StartInfo.WindowStyle = ProcessWindowStyle.Normal;

                    Console.WriteLine($"WorkingDirectory: {process1.StartInfo.WorkingDirectory}");

                    if (process1.Start())
                    {
                        Console.WriteLine($"The process was started via ShellExecute (PID: {process1.Id})");

                        // Give it time to launch
                        Thread.Sleep(3000);

                        if (!process1.HasExited)
                        {
                            Console.WriteLine("The application has been successfully launched and is working.");
                            return true;
                        }
                        else
                        {
                            Console.WriteLine($"The application terminated immediately (code: {process1.ExitCode})");
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error ShellExecute: {ex.Message}");
                }

                // Method 2: Using Process.Start with UseShellExecute = false
                Console.WriteLine("\nMethod 2: Launch directly");
                try
                {
                    var process2 = new Process();
                    process2.StartInfo.FileName = appExePath;
                    process2.StartInfo.UseShellExecute = false;
                    process2.StartInfo.WorkingDirectory = Path.GetDirectoryName(appExePath);
                    process2.StartInfo.CreateNoWindow = false;
                    process2.StartInfo.RedirectStandardOutput = false;

                    if (process2.Start())
                    {
                        Console.WriteLine($"The process was started directly (PID: {process2.Id})");

                        // Give it time to launch
                        Thread.Sleep(3000);

                        if (!process2.HasExited)
                        {
                            Console.WriteLine("Application launched successfully");
                            return true;
                        }
                        else
                        {
                            Console.WriteLine($"The application terminated immediately (code: {process2.ExitCode})");
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Direct launch error: {ex.Message}");
                }

                // Method 3: Launch via cmd
                Console.WriteLine("\nMethod 3: Launch via command line");
                try
                {
                    var process3 = new Process();
                    process3.StartInfo.FileName = "cmd.exe";
                    process3.StartInfo.Arguments = $"/c start \"\" \"{appExePath}\"";
                    process3.StartInfo.CreateNoWindow = true;
                    process3.StartInfo.UseShellExecute = false;

                    if (process3.Start())
                    {
                        Console.WriteLine("The launch command was sent via cmd");
                        Thread.Sleep(3000);
                        return true;
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error launching via cmd: {ex.Message}");
                }

                Console.WriteLine("All launch methods did not work.");
                return false;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Critical error on startup: {ex.Message}");
                Console.WriteLine($"StackTrace: {ex.StackTrace}");
                return false;
            }
        }

        private static List<string> GetPossibleAppPaths()
        {
            var appName = "LPStudio.exe";
            var possiblePaths = new List<string>();

            // 1. Current user path (AppData\Local\Programs)
            var userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            possiblePaths.Add(Path.Combine(userProfile, "AppData", "Local", "Programs", "LPStudio", appName));

            // 2. Public user (if installed for all users)
            var publicProfile = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            possiblePaths.Add(Path.Combine(publicProfile, "Programs", "LPStudio", appName));

            // 3. Current working directory
            possiblePaths.Add(Path.Combine(Directory.GetCurrentDirectory(), appName));

            // 4. Next to Updater
            var updaterDir = AppDomain.CurrentDomain.BaseDirectory;
            possiblePaths.Add(Path.Combine(updaterDir, appName));

            // 5. Check AppDir from arguments (may change)
            var appDirFromArgs = GetAppDirFromArgs();
            if (!string.IsNullOrEmpty(appDirFromArgs))
            {
                possiblePaths.Add(Path.Combine(appDirFromArgs, appName));
            }

            return possiblePaths;
        }

        private static bool TryLaunchViaDesktopShortcut()
        {
            try
            {
                var desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                var shortcutPath = Path.Combine(desktopPath, "LPStudio.lnk");

                if (File.Exists(shortcutPath))
                {
                    Console.WriteLine($"Shortcut found on desktop: {shortcutPath}");

                    var startInfo = new ProcessStartInfo
                    {
                        FileName = shortcutPath,
                        UseShellExecute = true,
                        WindowStyle = ProcessWindowStyle.Normal
                    };

                    Process.Start(startInfo);
                    Console.WriteLine("Shortcut launched");
                    return true;
                }
                else
                {
                    Console.WriteLine($"Shortcut not found: {shortcutPath}");

                    // Check in the Start menu
                    var startMenuPath = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.StartMenu),
                        "Programs",
                        "LPStudio.lnk"
                    );

                    if (File.Exists(startMenuPath))
                    {
                        Console.WriteLine($"Shortcut found in Start menu: {startMenuPath}");
                        Process.Start(startMenuPath);
                        return true;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error when launching via shortcut: {ex.Message}");
            }

            return false;
        }

        private static string GetAppDirFromArgs()
        {
            try
            {
                var args = Environment.GetCommandLineArgs();
                for (int i = 0; i < args.Length - 1; i++)
                {
                    if (args[i] == "--app-dir" || args[i] == "-app-dir")
                    {
                        return args[i + 1];
                    }
                }
            }
            catch
            {
                // Ignore errors
            }

            return null;
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

        private static void CleanUpTempFiles()
        {
            try
            {
                Console.WriteLine("Cleaning up temporary files...");

                // Delete .old files that may have been created by previous launches
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

                // Delete the temporary download folder
                var tempUpdateDir = Path.Combine(Path.GetTempPath(), "WindowsForms_Update");
                if (Directory.Exists(tempUpdateDir))
                {
                    try
                    {
                        Directory.Delete(tempUpdateDir, true);
                        Console.WriteLine($"  Deleted temp directory: {tempUpdateDir}");
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
