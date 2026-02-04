package com.example.lpstudio

import android.app.Activity
import android.app.DownloadManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import androidx.core.content.FileProvider
import androidx.core.content.ContextCompat;
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.concurrent.TimeUnit

class AndroidUpdateService(
    private val context: Context,
    private val owner: String = "Albel-coder",
    private val repo: String = "LegoPrinter"
) {

    companion object {
        private const val PREFS_NAME = "update_preferences"
        private const val KEY_SKIP_VERSION = "skip_version"
        private const val KEY_POSTPONE_UNTIL = "postpone_until"
        private const val KEY_LAST_CHECK = "last_check"
        private const val CACHE_DURATION_MS = 6 * 60 * 60 * 1000L // 6 часов
    }

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()

    private val manifestUrl =
        "https://github.com/$owner/$repo/releases/latest/download/update.json"

    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private var currentDownloadId: Long? = null
    private val downloadReceiver = DownloadCompleteReceiver()

    data class UpdateInfo(
        val isAvailable: Boolean = false,
        val isRequired: Boolean = false,
        val isCritical: Boolean = false,
        val isCompatible: Boolean = true,

        val currentVersion: String = "",
        val currentVersionCode: Int = 0,
        val currentBuild: Int = 0,

        val latestVersion: String = "",
        val latestVersionCode: Int = 0,
        val latestBuild: Int = 0,

        val downloadUrl: String = "",
        val releaseNotes: String = "",
        val fileSize: Long = 0,
        val checksum: String = "",
        val minAndroidApi: Int = 21,

        val assetName: String = ""
    )

    inner class DownloadCompleteReceiver : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1)

            if (downloadId == currentDownloadId) {
                verifyAndInstallApk(downloadId)
            }
        }
    }

    init {
        val filter = IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE)
        ContextCompat.registerReceiver(context, downloadReceiver, filter,ContextCompat.RECEIVER_EXPORTED);
    }

    fun cleanup() {
        try {
            context.unregisterReceiver(downloadReceiver)
        } catch (e: IllegalArgumentException) {
            // Receiver не был зарегистрирован, игнорируем
        }
    }

    suspend fun checkForUpdates(forceCheck: Boolean = false): UpdateInfo {
        // Проверяем кэш
        if (!forceCheck) {
            val lastCheck = prefs.getLong(KEY_LAST_CHECK, 0)
            if (System.currentTimeMillis() - lastCheck < CACHE_DURATION_MS) {
                return UpdateInfo() // Возвращаем пустой результат
            }
        }

        return try {
            // Получаем текущую версию приложения
            val packageInfo = getPackageInfo()
            val currentVersion = packageInfo.versionName ?: "1.0.0"
            val currentVersionCode = packageInfo.versionCode

            // Проверяем, нужно ли пропустить проверку
            if (!shouldCheckForUpdate(currentVersionCode)) {
                return UpdateInfo(
                    currentVersion = currentVersion,
                    currentVersionCode = currentVersionCode
                )
            }

            // Загружаем манифест
            val manifest = downloadManifest() ?: return UpdateInfo(
                currentVersion = currentVersion,
                currentVersionCode = currentVersionCode
            )

            // Парсим Android-специфичную информацию
            val androidInfo = parseAndroidPlatformInfo(manifest)

            // Проверяем совместимость
            val isCompatible = Build.VERSION.SDK_INT >= androidInfo.minApi

            if (!isCompatible) {
                return UpdateInfo(
                    currentVersion = currentVersion,
                    currentVersionCode = currentVersionCode,
                    isCompatible = false
                )
            }

            // Сравниваем версии
            val isUpdateAvailable = androidInfo.versionCode > currentVersionCode

            if (isUpdateAvailable) {
                UpdateInfo(
                    isAvailable = true,
                    isRequired = androidInfo.isRequired,
                    isCritical = androidInfo.isCritical,
                    isCompatible = true,

                    currentVersion = currentVersion,
                    currentVersionCode = currentVersionCode,
                    currentBuild = currentVersionCode,

                    latestVersion = androidInfo.version,
                    latestVersionCode = androidInfo.versionCode,
                    latestBuild = androidInfo.build,

                    downloadUrl = androidInfo.downloadUrl,
                    releaseNotes = androidInfo.changelog,
                    fileSize = androidInfo.fileSize,
                    checksum = androidInfo.sha256,
                    minAndroidApi = androidInfo.minApi,

                    assetName = "LPStudio-${androidInfo.version}.apk"
                )
            } else {
                UpdateInfo(
                    currentVersion = currentVersion,
                    currentVersionCode = currentVersionCode,
                    currentBuild = currentVersionCode
                )
            }.also {
                prefs.edit().putLong(KEY_LAST_CHECK, System.currentTimeMillis()).apply()
            }

        } catch (e: Exception) {
            UpdateInfo()
        }
    }

    private fun getPackageInfo(): PackageInfo {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.packageManager.getPackageInfo(
                context.packageName,
                PackageManager.PackageInfoFlags.of(0)
            )
        } else {
            @Suppress("DEPRECATION")
            context.packageManager.getPackageInfo(context.packageName, 0)
        }
    }

    private fun shouldCheckForUpdate(currentVersionCode: Int): Boolean {
        // Проверяем, не пропущена ли версия
        val skipVersionCode = prefs.getInt(KEY_SKIP_VERSION, -1)
        if (skipVersionCode > currentVersionCode) {
            return false
        }

        // Проверяем, не отложено ли обновление
        val postponeUntil = prefs.getLong(KEY_POSTPONE_UNTIL, 0)
        if (postponeUntil > System.currentTimeMillis()) {
            return false
        }

        return true
    }

    private suspend fun downloadManifest(): JSONObject? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url(manifestUrl)
                .header("User-Agent", "LPStudio-Android-Updater")
                .build()

            val response = httpClient.newCall(request).execute()

            if (response.isSuccessful) {
                val jsonString = response.body?.string()
                JSONObject(jsonString)
            } else {
                null
            }
        } catch (e: Exception) {
            null
        }
    }

    private data class AndroidPlatformInfo(
        val version: String,
        val versionCode: Int,
        val build: Int,
        val downloadUrl: String,
        val changelog: String,
        val fileSize: Long,
        val sha256: String,
        val minApi: Int,
        val isRequired: Boolean,
        val isCritical: Boolean
    )

    private fun parseAndroidPlatformInfo(manifest: JSONObject): AndroidPlatformInfo {
        val androidPlatform = manifest.getJSONObject("platforms")
            .getJSONObject("android")

        val versionString = androidPlatform.getString("version")
        val versionParts = versionString.split(".")

        return AndroidPlatformInfo(
            version = if (versionParts.size >= 3) {
                "${versionParts[0]}.${versionParts[1]}.${versionParts[2]}"
            } else {
                versionString
            },
            versionCode = androidPlatform.getInt("versionCode"),
            build = if (versionParts.size >= 4) {
                versionParts[3].toIntOrNull() ?: 0
            } else {
                0
            },
            downloadUrl = androidPlatform.getString("url"),
            changelog = androidPlatform.optString("changelog", ""),
            fileSize = androidPlatform.optLong("fileSize", 0),
            sha256 = androidPlatform.optString("sha256", ""),
            minApi = androidPlatform.optInt("minAndroidApi", 21),
            isRequired = androidPlatform.optBoolean("isRequired", false),
            isCritical = androidPlatform.optBoolean("isCritical", false)
        )
    }

    fun downloadAndInstall(updateInfo: UpdateInfo): Long {
        val appDir = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)
            ?: context.filesDir

        val fileName = updateInfo.assetName.ifEmpty { "LPStudio-update.apk" }
        val destinationFile = File(appDir, fileName)

        // Удаляем старый файл, если существует
        if (destinationFile.exists()) {
            destinationFile.delete()
        }

        val downloadManager = context.getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager

        val request = DownloadManager.Request(Uri.parse(updateInfo.downloadUrl))
            .setTitle("Обновление LPStudio")
            .setDescription("Загрузка версии ${updateInfo.latestVersion}")
            .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
            .setDestinationUri(Uri.fromFile(destinationFile))
            .setAllowedOverMetered(true)
            .setAllowedOverRoaming(false)

        if (updateInfo.fileSize > 0) {
            request.setAllowedNetworkTypes(
                DownloadManager.Request.NETWORK_WIFI or
                        DownloadManager.Request.NETWORK_MOBILE
            )
        }

        val downloadId = downloadManager.enqueue(request)
        currentDownloadId = downloadId

        // Сохраняем информацию о загрузке для проверки checksum
        prefs.edit()
            .putLong("current_download_id", downloadId)
            .putString("expected_checksum", updateInfo.checksum)
            .putString("download_file_path", destinationFile.absolutePath)
            .apply()

        return downloadId
    }

    fun verifyAndInstallApk(downloadId: Long): Boolean {
        val downloadManager = context.getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager

        return try {
            // Получаем Uri напрямую через DownloadManager
            val uri = downloadManager.getUriForDownloadedFile(downloadId)
            if (uri == null) {
                return false
            }

            // Проверяем, что файл существует
            val file = getFileFromUri(uri) ?: return false

            // Проверяем checksum
            val expectedChecksum = prefs.getString("expected_checksum", "")
            if (expectedChecksum.isNullOrEmpty() || verifyChecksum(file, expectedChecksum)) {
                installApk(file)
                true
            } else {
                file.delete()
                false
            }
        } catch (e: SecurityException) {
            // Нет разрешения на доступ к файлу
            false
        } catch (e: Exception) {
            false
        }
    }

    // Более простой метод получения файла из Uri
    private fun getFileFromUri(uri: Uri): File? {
        return try {
            // Открываем файловый дескриптор
            val parcelFileDescriptor = context.contentResolver.openFileDescriptor(uri, "r")
            val fileDescriptor = parcelFileDescriptor?.fileDescriptor ?: return null

            // Создаём временный файл и копируем содержимое
            val tempFile = File.createTempFile("update_", ".apk", context.cacheDir)

            FileInputStream(fileDescriptor).use { input ->
                FileOutputStream(tempFile).use { output ->
                    input.copyTo(output)
                }
            }

            parcelFileDescriptor.close()
            tempFile
        } catch (e: Exception) {
            null
        }
    }


    private fun verifyChecksum(file: File, expectedHash: String): Boolean {
        if (expectedHash.isEmpty()) return true

        return try {
            val digest = MessageDigest.getInstance("SHA-256")
            file.inputStream().use { input ->
                val buffer = ByteArray(8192)
                var read: Int
                while (input.read(buffer).also { read = it } > 0) {
                    digest.update(buffer, 0, read)
                }
            }

            val hashBytes = digest.digest()
            val actualHash = hashBytes.joinToString("") { "%02x".format(it) }

            actualHash.equals(expectedHash, ignoreCase = true)
        } catch (e: Exception) {
            false
        }
    }

    private fun installApk(file: File) {
        val uri = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            FileProvider.getUriForFile(
                context,
                "${context.packageName}.provider",
                file
            )
        } else {
            Uri.fromFile(file)
        }

        val installIntent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
        }

        context.startActivity(installIntent)
    }

    fun skipVersion(versionCode: Int) {
        prefs.edit().putInt(KEY_SKIP_VERSION, versionCode).apply()
    }

    fun postponeUpdate(hours: Int) {
        val postponeUntil = System.currentTimeMillis() + hours * 60 * 60 * 1000L
        prefs.edit().putLong(KEY_POSTPONE_UNTIL, postponeUntil).apply()
    }

    fun clearSkipVersion() {
        prefs.edit().remove(KEY_SKIP_VERSION).apply()
    }
}
