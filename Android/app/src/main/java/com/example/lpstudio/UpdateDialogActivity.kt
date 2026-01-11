package com.example.lpstudio

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

class UpdateDialogActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val releaseInfo = intent.getSerializableExtra("release_info") as? UpdateService.ReleaseInfo

        if (releaseInfo == null) {
            finish()
            return
        }

        showUpdateDialog(releaseInfo)
    }

    private fun showUpdateDialog(releaseInfo: UpdateService.ReleaseInfo) {
        val view = layoutInflater.inflate(R.layout.dialog_update, null)

        val titleTextView = view.findViewById<TextView>(R.id.update_title)
        val currentVersionTextView = view.findViewById<TextView>(R.id.current_version)
        val latestVersionTextView = view.findViewById<TextView>(R.id.latest_version)
        val releaseNameTextView = view.findViewById<TextView>(R.id.release_name)
        val releaseNotesTextView = view.findViewById<TextView>(R.id.release_notes)
        val autoUpdateCheckBox = view.findViewById<CheckBox>(R.id.auto_update)

        titleTextView.text = "Доступна новая версия ${releaseInfo.version}"
        currentVersionTextView.text = "Текущая версия: ${getCurrentVersion()}"
        latestVersionTextView.text = "Новая версия: ${releaseInfo.version}"
        releaseNameTextView.text = releaseInfo.name
        releaseNotesTextView.text = releaseInfo.notes

        val dialog = AlertDialog.Builder(this)
            .setView(view)
            .setPositiveButton("Установить") { _, _ ->
                startDownload(releaseInfo)
                finish()
            }
            .setNegativeButton("Позже") { _, _ ->
                // Save the setting to postpone for 1 day
                val prefs = getSharedPreferences("updates", MODE_PRIVATE)
                prefs.edit().putLong("postpone_until", System.currentTimeMillis() + 86400000).apply()
                finish()
            }
            .setNeutralButton("Пропустить") { _, _ ->
                // Save the missing version
                val prefs = getSharedPreferences("updates", MODE_PRIVATE)
                prefs.edit().putString("skip_version", releaseInfo.version).apply()
                finish()
            }
            .setOnCancelListener {
                finish()
            }
            .create()

        dialog.show()
    }

    private fun getCurrentVersion(): String {
        return packageManager.getPackageInfo(packageName, 0).versionName
    }

    private fun startDownload(releaseInfo: UpdateService.ReleaseInfo) {
        val intent = Intent(this, UpdateService::class.java).apply {
            action = UpdateService.ACTION_DOWNLOAD_UPDATE
            putExtra("download_url", releaseInfo.apkUrl)
            putExtra("version", releaseInfo.version)
        }
        startService(intent)
    }
}
