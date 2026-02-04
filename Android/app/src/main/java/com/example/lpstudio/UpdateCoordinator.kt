package com.example.lpstudio

import android.content.Context
import com.example.lpstudio.AndroidUpdateService
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class UpdateCoordinator(private val context: Context) {

    private val updateService = AndroidUpdateService(context)
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    private val _updateState = MutableStateFlow<UpdateState>(UpdateState.Idle)
    val updateState: StateFlow<UpdateState> =_updateState

    sealed class UpdateState {
        object Idle : UpdateState()
        object Checking : UpdateState()
        data class Available(val info: AndroidUpdateService.UpdateInfo) : UpdateState()
        data class Downloading(val progress: Float) : UpdateState()
        data class Installing(val info: AndroidUpdateService.UpdateInfo) : UpdateState()
        data class Error(val message: String) : UpdateState()
    }

    fun checkForUpdates(force: Boolean = false) {
        scope.launch {
            _updateState.value = UpdateState.Checking

            try {
                val updateInfo = withContext(Dispatchers.IO) {
                    updateService.checkForUpdates(force)
                }

                if (updateInfo.isAvailable) {
                    _updateState.value = UpdateState.Available(updateInfo)
                }
                else {
                    _updateState.value = UpdateState.Idle
                }
            }
            catch(e: Exception) {
                _updateState.value = UpdateState.Error("Error checking: ${e.message}")
            }
        }
    }

    fun downloadUpdate(info: AndroidUpdateService.UpdateInfo) {
        scope.launch {
            try {
                val downloadId = withContext(Dispatchers.IO) {
                    updateService.downloadAndInstall(info)
                }
                _updateState.value = UpdateState.Downloading(0f)
            }
            catch(e: Exception) {
                _updateState.value = UpdateState.Error("Error downloading: ${e.message}")
            }
        }
    }

    fun skipVersion(versionCode: Int) {
        updateService.skipVersion(versionCode)
        _updateState.value = UpdateState.Idle
    }

    fun postponeUpdate(hours: Int) {
        updateService.postponeUpdate(hours)
        _updateState.value = UpdateState.Idle
    }

    fun cleanup() {
        scope.cancel()
        updateService.cleanup()
    }
}