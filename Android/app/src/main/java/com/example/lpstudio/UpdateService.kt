package com.example.lpstudio

import android.app.*
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.content.FileProvider
import kotlinx.coroutines.*
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.net.URL

class UpdateService : Service() {

    companion object {
        const val ACTION_CHECK_UPDATE = "ACTION_CHECK_UPDATE"
        const val ACTION_DOWNLOAD_UPDATE = "ACTION_DOWNLOAD_UPDATE"
        const val ACTION_INSTALL_UPDATE = "ACTION_INSTALL_UPDATE"

        private const val GITHUB_OWNER = "Albel-coder"
        private const val GITHUB_REPO = "LegoPrinter"
        private const val GITHUB_API = "https://api.github.com"

        private const val CHANNEL_ID = "update_channel"
        private const val NOTIFICATION_ID = 1001
    }

    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_CHECK_UPDATE -> checkForUpdates()
            ACTION_DOWNLOAD_UPDATE -> {
                val downloadUrl = intent.getStringExtra("download_url")
                val version = intent.getStringExtra("version")
                if (downloadUrl != null && version != null) {
                    downloadUpdate(downloadUrl, version)
                }
            }
            ACTION_INSTALL_UPDATE -> {
                val apkPath = intent.getStringExtra("apk_path")
                if (apkPath != null) {
                    installUpdate(apkPath)
                }
            }
        }
        return START_STICKY
    }

    private fun checkForUpdates() {
        serviceScope.launch {
            try {
                val currentVersion = packageManager.getPackageInfo(packageName, 0).versionName
                val latestVersion = getLatestVersion()

                if (isNewerVersion(latestVersion, currentVersion)) {
                    val releaseInfo = getReleaseInfo()
                    showUpdateNotification(releaseInfo)
                }
            } catch (e: Exception) {
                Log.e("UpdateService", "Error checking updates: ${e.message}")
            }
        }
    }

    private suspend fun getLatestVersion(): String {
        val url = "$GITHUB_API/repos/$GITHUB_OWNER/$GITHUB_REPO/releases/latest"
        val client = OkHttpClient()
        val request = Request.Builder()
            .url(url)
            .addHeader("User-Agent", "LegoPrinter-Android-Updater")
            .build()

        val response = client.newCall(request).execute()
        val json = JSONObject(response.body?.string() ?: "{}")
        return json.getString("tag_name").replace("v", "")
    }

    private suspend fun getReleaseInfo(): ReleaseInfo {
        val url = "$GITHUB_API/repos/$GITHUB_OWNER/$GITHUB_REPO/releases/latest"
        val client = OkHttpClient()
        val request = Request.Builder()
            .url(url)
            .addHeader("User-Agent", "LegoPrinter-Android-Updater")
            .build()

        val response = client.newCall(request).execute()
        val json = JSONObject(response.body?.string() ?: "{}")

        val assets = json.getJSONArray("assets")
        var apkUrl = ""
        var apkName = ""

        for (i in 0 until assets.length()) {
            val asset = assets.getJSONObject(i)
            val name = asset.getString("name")
            if (name.endsWith(".apk")) {
                apkUrl = asset.getString("browser_download_url")
                apkName = name
                break
            }
        }

        return ReleaseInfo(
            version = json.getString("tag_name").replace("v", ""),
            name = json.getString("name"),
            notes = json.getString("body"),
            apkUrl = apkUrl,
            apkName = apkName,
            publishedAt = json.getString("published_at"),
            isPrerelease = json.getBoolean("prerelease")
        )
    }

    private fun isNewerVersion(newVersion: String, currentVersion: String): Boolean {
        val newParts = newVersion.split(".")
        val currentParts = currentVersion.split(".")

        for (i in 0 until minOf(newParts.size, currentParts.size)) {
            val newPart = newParts[i].toIntOrNull() ?: 0
            val currentPart = currentParts[i].toIntOrNull() ?: 0

            if (newPart > currentPart) return true
            if (newPart < currentPart) return false
        }

        return newParts.size > currentParts.size
    }

    private fun showUpdateNotification(releaseInfo: ReleaseInfo) {
        createNotificationChannel()

        val notificationIntent = Intent(this, UpdateDialogActivity::class.java).apply {
            putExtra("release_info", releaseInfo)
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
        }

        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            notificationIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Доступно обновление ${releaseInfo.version}")
            .setContentText(releaseInfo.name)
            .setSmallIcon(R.drawable.ic_update)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .build()

        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(NOTIFICATION_ID, notification)
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Обновления приложения",
                NotificationManager.IMPORTANCE_HIGH
            ).apply {
                description = "Уведомления о доступных обновлениях"
            }

            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager.createNotificationChannel(channel)
        }
    }

    private fun downloadUpdate(downloadUrl: String, version: String) {
        serviceScope.launch {
            try {
                // Show a loading notification
                showDownloadNotification(0, version)

                val url = URL(downloadUrl)
                val connection = url.openConnection()
                connection.connect()

                val inputStream = connection.getInputStream()
                val contentLength = connection.contentLength

                val downloadsDir = getExternalFilesDir("downloads")
                val apkFile = File(downloadsDir, "LegoPrinter_$version.apk")

                FileOutputStream(apkFile).use { outputStream ->
                    val buffer = ByteArray(8192)
                    var bytesRead: Int
                    var totalRead = 0L

                    while (inputStream.read(buffer).also { bytesRead = it } != -1) {
                        outputStream.write(buffer, 0, bytesRead)
                        totalRead += bytesRead

                        // Update progress every 5%
                        if (contentLength > 0) {
                            val progress = (totalRead * 100 / contentLength).toInt()
                            if (progress % 5 == 0) {
                                showDownloadNotification(progress, version)
                            }
                        }
                    }
                }

                // Show a notification about successful download
                showDownloadCompleteNotification(apkFile.absolutePath, version)

            } catch (e: Exception) {
                Log.e("UpdateService", "Download error: ${e.message}")
                showDownloadErrorNotification()
            }
        }
    }

    private fun showDownloadNotification(progress: Int, version: String) {
        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Загрузка обновления $version")
            .setContentText("$progress% завершено")
            .setSmallIcon(R.drawable.ic_download)
            .setProgress(100, progress, false)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setOngoing(true)
            .build()

        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(NOTIFICATION_ID, notification)
    }

    private fun showDownloadCompleteNotification(apkPath: String, version: String) {
        val installIntent = Intent(this, UpdateService::class.java).apply {
            action = ACTION_INSTALL_UPDATE
            putExtra("apk_path", apkPath)
        }

        val installPendingIntent = PendingIntent.getService(
            this,
            0,
            installIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Обновление $version загружено")
            .setContentText("Нажмите для установки")
            .setSmallIcon(R.drawable.ic_install)
            .addAction(
                NotificationCompat.Action.Builder(
                    R.drawable.ic_install,
                    "Установить",
                    installPendingIntent
                ).build()
            )
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
            .build()

        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(NOTIFICATION_ID, notification)
    }

    private fun showDownloadErrorNotification() {
        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Ошибка загрузки")
            .setContentText("Не удалось загрузить обновление")
            .setSmallIcon(R.drawable.ic_error)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
            .build()

        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(NOTIFICATION_ID, notification)
    }

    private fun installUpdate(apkPath: String) {
        val apkFile = File(apkPath)
        if (!apkFile.exists()) {
            Log.e("UpdateService", "APK file not found: $apkPath")
            return
        }

        val uri = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            FileProvider.getUriForFile(
                this,
                "${packageName}.provider",
                apkFile
            )
        } else {
            Uri.fromFile(apkFile)
        }

        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }

        startActivity(intent)
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceJob.cancel()
    }

    data class ReleaseInfo(
        val version: String,
        val name: String,
        val notes: String,
        val apkUrl: String,
        val apkName: String,
        val publishedAt: String,
        val isPrerelease: Boolean
    )
}