package com.example.lpstudio;

import android.app.DownloadManager;
import android.content.*;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;
import androidx.core.content.FileProvider;
import org.json.JSONObject;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Scanner;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import androidx.core.content.ContextCompat;
import android.Manifest;

public class AndroidUpdateService {
    private static final String TAG = "AndroidUpdateService";
    private static final String PREFS_NAME = "update_settings";
    private static final String KEY_SKIP_VERSION = "skip_version";
    private static final String KEY_POSTPONE_UNTIL = "postpone_until";
    private static final String KEY_LAST_CHECK = "last_check";
    private static final long CACHE_DURATION_MS = 6 * 60 * 60 * 1000L; // 6 hours

    private final Context context;
    private final Handler mainHandler;
    private final ExecutorService executorService;
    private final SharedPreferences prefs;
    private Long currentDownloadId = null;
    private final DownloadCompleteReceiver downloadReceiver;

    private String owner = "Albel-coder";
    private String repo = "LegoPrinter";
    private String manifestUrl;

    public AndroidUpdateService(Context context) {
        this(context, "Albel-coder", "LegoPrinter");
    }

    public AndroidUpdateService(Context context, String owner, String repo) {
        this.context = context;
        this.owner = owner;
        this.repo = repo;
        this.manifestUrl = String.format(
                "https://github.com/%s/%s/releases/latest/download/update.json",
                owner, repo
        );

        this.mainHandler = new Handler(Looper.getMainLooper());
        this.executorService = Executors.newSingleThreadExecutor();
        this.prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        this.downloadReceiver = new DownloadCompleteReceiver();

        registerReceiver();
    }

    private void registerReceiver() {
        IntentFilter filter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.registerReceiver(
                    downloadReceiver,
                    filter,
                    Context.RECEIVER_NOT_EXPORTED
            );
        } else {
            ContextCompat.registerReceiver(
                    context,
                    downloadReceiver,
                    new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE),
                    ContextCompat.RECEIVER_EXPORTED
            );
        }
    }

    public void checkForUpdates(UpdateCheckCallback callback) {
        executorService.execute(() -> {
            try {
                UpdateInfo updateInfo = checkForUpdatesBlocking();
                mainHandler.post(() -> callback.onResult(updateInfo));
            } catch (Exception e) {
                mainHandler.post(() -> callback.onError(e.getMessage()));
            }
        });
    }

    private UpdateInfo checkForUpdatesBlocking() throws Exception {
        // Check the cache
//        long lastCheck = prefs.getLong(KEY_LAST_CHECK, 0);
//        if (System.currentTimeMillis() - lastCheck < CACHE_DURATION_MS) {
//            return createNoUpdateInfo();
//        }

        // Get the current version
        PackageInfo packageInfo = context.getPackageManager()
                .getPackageInfo(context.getPackageName(), 0);

        UpdateInfo updateInfo = new UpdateInfo();
        updateInfo.currentVersion = packageInfo.versionName != null ?
                packageInfo.versionName : "1.0.0";

        // Correct versionCode handling for all APIs
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            updateInfo.currentVersionCode = (int) packageInfo.getLongVersionCode();
        } else {
            updateInfo.currentVersionCode = packageInfo.versionCode;
        }

        // Check if the check should be skipped
        if (!shouldCheckForUpdate(updateInfo.currentVersionCode)) {
            // ALWAYS save the check time, even if an update is missed
            prefs.edit().putLong(KEY_LAST_CHECK, System.currentTimeMillis()).apply();
            return updateInfo;
        }

        // Load the manifest
        String manifestJson = downloadManifest();
        if (manifestJson == null) {
            // Save the verification time even if there is an error
            prefs.edit().putLong(KEY_LAST_CHECK, System.currentTimeMillis()).apply();
            return updateInfo;
        }

        // Parse JSON
        JSONObject manifest = new JSONObject(manifestJson);
        JSONObject platforms = manifest.getJSONObject("platforms");

        if (!platforms.has("android")) {
            prefs.edit().putLong(KEY_LAST_CHECK, System.currentTimeMillis()).apply();
            return updateInfo;
        }

        JSONObject androidPlatform = platforms.getJSONObject("android");

        // Parse the version from the manifest
        String latestVersionString = androidPlatform.getString("version");
        int latestVersionCode = androidPlatform.optInt("versionCode", 0);

        if (latestVersionCode == 0) {
            latestVersionCode = versionStringToCode(latestVersionString);
            Log.d("latest version == 0", "using string to code ");
            Log.d("UpdateCheck", "Current: " + updateInfo.currentVersion + "/" + updateInfo.currentVersionCode +
                    ", Latest: " + updateInfo.latestVersion + "/" + updateInfo.latestBuild);

        }

        updateInfo.latestVersionCode = latestVersionCode;
        updateInfo.latestVersion = parseVersionName(latestVersionString);
        Log.d("latest version == 0", "executing: updateInfo.latestVersionCode = latestVersionCode;\n" +
                "        updateInfo.latestVersion = parseVersionName(latestVersionString); ");
        Log.d("UpdateCheck", "Current: " + updateInfo.currentVersion + "/" + updateInfo.currentVersionCode +
                ", Latest: " + updateInfo.latestVersion + "/" + updateInfo.latestBuild);

        // Compare versions
        boolean isUpdateAvailable = latestVersionCode > updateInfo.currentVersionCode;

        // ALWAYS save the check time (fixed)
        prefs.edit().putLong(KEY_LAST_CHECK, System.currentTimeMillis()).apply();

        if (isUpdateAvailable) {
            updateInfo.isAvailable = true;
            updateInfo.latestVersion = parseVersionName(latestVersionString);
            updateInfo.latestVersionCode = latestVersionCode;
            updateInfo.releaseNotes = androidPlatform.optString("changelog", "");
            updateInfo.downloadUrl = androidPlatform.getString("url");
            updateInfo.fileSize = androidPlatform.optLong("fileSize", 0);
            updateInfo.checksum = androidPlatform.optString("sha256", "");
            updateInfo.isRequired = androidPlatform.optBoolean("isRequired", false);
            updateInfo.isCritical = androidPlatform.optBoolean("isCritical", false);
        }

        return updateInfo;
    }

    private int versionStringToCode(String version) {
        String[] parts = version.split("\\.");
        int major = parts.length > 0 ? parsePart(parts[0]) : 0;
        int minor = parts.length > 1 ? parsePart(parts[1]) : 0;
        int patch = parts.length > 2 ? parsePart(parts[2]) : 0;
        int build = parts.length > 3 ? parsePart(parts[3]) : 0;

        // Gradle schema: major*10000000 + minor*100000 + patch*1000 + build
        return major * 10000000 + minor * 100000 + patch * 1000 + build;
    }

    private int parsePart(String part) {
        try {
            return Integer.parseInt(part);
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    private boolean shouldCheckForUpdate(int currentVersionCode) {
        int skipVersion = prefs.getInt(KEY_SKIP_VERSION, -1);
        if (skipVersion > currentVersionCode) {
            return false;
        }

        long postponeUntil = prefs.getLong(KEY_POSTPONE_UNTIL, 0);
        if (postponeUntil > System.currentTimeMillis()) {
            return false;
        }

        return true;
    }

    private String parseVersionName(String versionString) {
        String[] parts = versionString.split("\\.");
        if (parts.length >= 3) {
            return parts[0] + "." + parts[1] + "." + parts[2];
        }
        return versionString;
    }

    private String downloadManifest() {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(manifestUrl);
            connection = (HttpURLConnection) url.openConnection();
            connection.setRequestMethod("GET");
            connection.setRequestProperty("User-Agent", "LPStudio-Android-Updater");
            connection.setConnectTimeout(30000);
            connection.setReadTimeout(30000);

            int responseCode = connection.getResponseCode();
            if (responseCode == 200) {
                InputStream inputStream = connection.getInputStream();
                Scanner scanner = new Scanner(inputStream).useDelimiter("\\A");
                return scanner.hasNext() ? scanner.next() : null;
            }
            else {
                callback.onError("Error loading manifest: HTTP " + responseCode);
            }
        } catch (Exception e) {
            callback.onError("Error downloading manifest: " + e.getMessage());
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
        return null;
    }

    private UpdateInfo createNoUpdateInfo() {
        try {
            PackageInfo packageInfo = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0);

            UpdateInfo info = new UpdateInfo();
            info.currentVersion = packageInfo.versionName != null ?
                    packageInfo.versionName : "1.0.0";

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                info.currentVersionCode = (int) packageInfo.getLongVersionCode();
            } else {
                info.currentVersionCode = packageInfo.versionCode;
            }
            return info;
        } catch (Exception e) {
            UpdateInfo info = new UpdateInfo();
            info.currentVersion = "1.0.0";
            info.currentVersionCode = 1;
            return info;
        }
    }

    public long downloadUpdate(UpdateInfo updateInfo) {
        DownloadManager downloadManager = (DownloadManager)
                context.getSystemService(Context.DOWNLOAD_SERVICE);

        // We work only with the internal cache, without external storage
        File downloadsDir = new File(context.getCacheDir(), "downloads");
        downloadsDir.mkdirs();

        String fileName = "LPStudio.apk"; // take a fixed name from JSON
        String fileUri = updateInfo.downloadUrl;
        fileName = fileUri.substring(fileUri.lastIndexOf('/') + 1);
        DownloadManager.Request request = new DownloadManager.Request(Uri.parse(updateInfo.downloadUrl))
                .setTitle("LPStudio update")
                .setDescription("Downloading the update")
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                .setDestinationInExternalFilesDir(context, null, fileName)
                .setAllowedOverMetered(true)
                .setAllowedOverRoaming(false);

        if (updateInfo.fileSize > 0) {
            request.setAllowedNetworkTypes(
                    DownloadManager.Request.NETWORK_WIFI |
                            DownloadManager.Request.NETWORK_MOBILE
            );
        }

        long downloadId = downloadManager.enqueue(request);
        currentDownloadId = downloadId;

        // Save information for checksum verification
        prefs.edit()
                .putLong("current_download_id", downloadId)
                .putString("expected_checksum", updateInfo.checksum)
                .apply();

        return downloadId;
    }

    public void skipVersion(int versionCode) {
        prefs.edit().putInt(KEY_SKIP_VERSION, versionCode).apply();
    }

    public void postponeUpdate(int hours) {
        long postponeUntil = System.currentTimeMillis() + (hours * 60 * 60 * 1000L);
        prefs.edit().putLong(KEY_POSTPONE_UNTIL, postponeUntil).apply();
    }

    public void clearSkipVersion() {
        prefs.edit().remove(KEY_SKIP_VERSION).apply();
    }

    public void cleanup() {
        try {
            context.unregisterReceiver(downloadReceiver);
        } catch (IllegalArgumentException e) {
            // Receiver was not registered
        }
        executorService.shutdown();
    }

    // Method for installing APK
    private void installApk(File apkFile) {
        try {
            Uri apkUri;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                apkUri = FileProvider.getUriForFile(
                        context,
                        context.getPackageName() + ".provider",
                        apkFile
                );
            } else {
                apkUri = Uri.fromFile(apkFile);
            }

            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setDataAndType(apkUri, "application/vnd.android.package-archive");
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            }

            // Check if the system can handle the installation
            if (intent.resolveActivity(context.getPackageManager()) != null) {
                context.startActivity(intent);
            } else {
                mainHandler.post(() -> {
                    Toast.makeText(context,
                            "Failed to start installation. Check your file manager.",
                            Toast.LENGTH_LONG).show();
                });
            }
        } catch (Exception e) {
            Log.e(TAG, "Install APK error: " + e.getMessage());
            mainHandler.post(() -> {
                Toast.makeText(context,
                        "Installation error: " + e.getMessage(),
                        Toast.LENGTH_LONG).show();
            });
        }
    }

    // Checksum verification
    private boolean verifyChecksum(File file, String expectedHash) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            FileInputStream fis = new FileInputStream(file);
            byte[] buffer = new byte[8192];
            int bytesRead;

            while ((bytesRead = fis.read(buffer)) != -1) {
                digest.update(buffer, 0, bytesRead);
            }
            fis.close();

            byte[] hashBytes = digest.digest();
            StringBuilder sb = new StringBuilder();
            for (byte b : hashBytes) {
                sb.append(String.format("%02x", b));
            }
            String actualHash = sb.toString();

            return actualHash.equals(expectedHash.toLowerCase());
        } catch (Exception e) {
            Log.e(TAG, "Checksum verification error: " + e.getMessage());
            return false;
        }
    }

    public static class UpdateInfo {
        public boolean isAvailable = false;
        public boolean isRequired = false;
        public boolean isCritical = false;
        public boolean isCompatible = true;

        public String currentVersion = "";
        public int currentVersionCode = 0;
        public int currentBuild = 0;

        public String latestVersion = "";
        public int latestVersionCode = 0;
        public int latestBuild = 0;

        public String downloadUrl = "";
        public String releaseNotes = "";
        public long fileSize = 0;
        public String checksum = "";
        public int minAndroidApi = 21;

        public String assetName = "";
    }

    public interface UpdateCheckCallback {
        void onResult(UpdateInfo updateInfo);
        void onError(String error);
    }

    // Fixed DownloadCompleteReceiver with real logic
    class DownloadCompleteReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            long downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1);

            if (downloadId == currentDownloadId) {
                Log.i(TAG, "Download completed: " + downloadId);

                // Get the file path and the expected checksum
                String filePath = prefs.getString("download_file_path", null);
                String expectedChecksum = prefs.getString("expected_checksum", null);

                if (filePath != null) {
                    File apkFile = new File(filePath);
                    if (apkFile.exists()) {
                        // Check the checksum
                        if (expectedChecksum != null && !expectedChecksum.isEmpty()) {
                            if (verifyChecksum(apkFile, expectedChecksum)) {
                                // Run the installation
                                installApk(apkFile);
                                // Show the notification
                                mainHandler.post(() -> {
                                    Toast.makeText(context,
                                            "Download complete. Beginning installation...",
                                            Toast.LENGTH_LONG).show();
                                });
                            } else {
                                Log.e(TAG, "Checksum mismatch!");
                                mainHandler.post(() -> {
                                    Toast.makeText(context,
                                            "Error: Checksum does not match. File is corrupted.",
                                            Toast.LENGTH_LONG).show();
                                });
                            }
                        } else {
                            // If checksum is not specified, just set
                            installApk(apkFile);
                            mainHandler.post(() -> {
                                Toast.makeText(context,
                                        "Download complete. Beginning installation...",
                                        Toast.LENGTH_LONG).show();
                            });
                        }
                    } else {
                        mainHandler.post(() -> {
                            Toast.makeText(context,
                                    "Error: File not found",
                                    Toast.LENGTH_LONG).show();
                        });
                    }
                }
            }
        }
    }

    private UpdateCallback callback;

    public interface UpdateCallback {
        void onError(String message);
        void onInfo(String message);
    }

    public void setUpdateCallback(UpdateCallback callback) {
        this.callback = callback;
    }
}
