package com.example.lpstudio

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences

object UpdateHelper {

    private const val PREFS_NAME = "update_preferences"
    private const val KEY_AUTO_UPDATE = "auto_update"
    private const val KEY_SKIP_VERSION = "skip_version"
    private const val KEY_POSTPONE_UNTIL = "postpone_until"

    fun checkForUpdates(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

        // Check the settings
        if (!prefs.getBoolean(KEY_AUTO_UPDATE, true)) return

        val skipVersion = prefs.getString(KEY_SKIP_VERSION, null)
        val postponeUntil = prefs.getLong(KEY_POSTPONE_UNTIL, 0)

        if (postponeUntil > System.currentTimeMillis()) return

        // Start the update check service
        val intent = Intent(context, UpdateService::class.java)
        intent.action = UpdateService.ACTION_CHECK_UPDATE
        context.startService(intent)
    }

    fun setAutoUpdateEnabled(context: Context, enabled: Boolean) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putBoolean(KEY_AUTO_UPDATE, enabled).apply()
    }

    fun setSkipVersion(context: Context, version: String) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString(KEY_SKIP_VERSION, version).apply()
    }

    fun setPostponeUntil(context: Context, timestamp: Long) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putLong(KEY_POSTPONE_UNTIL, timestamp).apply()
    }

    fun getCurrentVersion(context: Context): String {
        return try {
            val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
            packageInfo.versionName
        } catch (e: Exception) {
            "1.0.0.0"
        }
    }
}