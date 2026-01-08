package com.example.printerapp

import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.printerapp.databinding.ActivityMainBinding
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var printer: PrinterController
    private val scope = CoroutineScope(Dispatchers.Main)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Инициализация принтера
        printer = PrinterController()

        setupUI()
        setupListeners()
        updateConnectionStatus()
    }

    private fun setupUI() {
        // Можно добавить анимации, стили и т.д.
    }

    private fun setupListeners() {
        binding.btnConnect.setOnClickListener {
            connectPrinter()
        }

        binding.btnDisconnect.setOnClickListener {
            disconnectPrinter()
        }

        binding.btnSendCommand.setOnClickListener {
            sendCommand()
        }

        binding.btnTest.setOnClickListener {
            runTest()
        }

        binding.btnBattery.setOnClickListener {
            checkBattery()
        }

        binding.btnRefreshLogs.setOnClickListener {
            refreshLogs()
        }

        binding.btnClearLogs.setOnClickListener {
            clearLogs()
        }

        binding.btnInfo.setOnClickListener {
            showConnectionInfo()
        }
    }

    private fun connectPrinter() {
        scope.launch {
            binding.progressBar.visibility = android.view.View.VISIBLE
            try {
                val connected = withContext(Dispatchers.IO) {
                    printer.connect()
                }
                if (connected) {
                    showToast("Connected successfully!")
                    updateConnectionStatus()
                } else {
                    showToast("Connection failed: ${printer.lastErrorMessage}")
                }
            } catch (e: Exception) {
                showToast("Connection error: ${e.message}")
            } finally {
                binding.progressBar.visibility = android.view.View.GONE
            }
        }
    }

    private fun disconnectPrinter() {
        scope.launch {
            try {
                val disconnected = withContext(Dispatchers.IO) {
                    printer.disconnect()
                }
                if (disconnected) {
                    showToast("Disconnected")
                    updateConnectionStatus()
                }
            } catch (e: Exception) {
                showToast("Disconnect error: ${e.message}")
            }
        }
    }

    private fun sendCommand() {
        val commandText = binding.etCommand.text.toString().trim()
        if (commandText.isEmpty()) {
            showToast("Enter command")
            return
        }

        scope.launch {
            try {
                val command = hexStringToByteArray(commandText)
                withContext(Dispatchers.IO) {
                    printer.sendCommand(command)
                }
                showToast("Command sent")
                binding.etCommand.text.clear()
                refreshLogs()
            } catch (e: Exception) {
                showToast("Error: ${e.message}")
            }
        }
    }

    private fun runTest() {
        scope.launch {

            try {
                withContext(Dispatchers.IO) {
                    printer.test()
                }
                showToast("Test executed")
                refreshLogs()
            } catch (e: Exception) {
                showToast("Test error: ${e.message}")
            }
        }
    }

    private fun checkBattery() {
        scope.launch {
            binding.progressBar.visibility = android.view.View.VISIBLE
            try {
                val requested = withContext(Dispatchers.IO) {
                    printer.requestBatteryLevel()
                }

                if (requested) {
                    // Даем время на обновление
                    kotlinx.coroutines.delay(1000)

                    val level = withContext(Dispatchers.IO) {
                        printer.batteryLevel
                    }
                    val isFresh = withContext(Dispatchers.IO) {
                        printer.isBatteryLevelFresh(30)
                    }

                    binding.tvBattery.text = "Battery: $level% (Fresh: $isFresh)"
                } else {
                    showToast("Failed to request battery level")
                }
            } catch (e: Exception) {
                showToast("Battery error: ${e.message}")
            } finally {
                binding.progressBar.visibility = android.view.View.GONE
            }
        }
    }

    private fun refreshLogs() {
        scope.launch {
            try {
                val logs = withContext(Dispatchers.IO) {
                    printer.allLogs
                }
                val logText = logs.joinToString("\n")
                binding.tvLogs.text = logText

                // Автоматическая прокрутка вниз
                binding.scrollView.post {
                    binding.scrollView.fullScroll(android.view.View.FOCUS_DOWN)
                }
            } catch (e: Exception) {
                Log.e("MainActivity", "Error refreshing logs: ${e.message}")
            }
        }
    }

    private fun clearLogs() {
        scope.launch {
            withContext(Dispatchers.IO) {
                printer.clearLog()
            }
            binding.tvLogs.text = ""
            showToast("Logs cleared")
        }
    }

    private fun showConnectionInfo() {
        scope.launch {
            try {
                val info = withContext(Dispatchers.IO) {
                    printer.connectionInfo
                }
                showToast("Connection info: $info")
                refreshLogs()
            } catch (e: Exception) {
                showToast("Error getting connection info: ${e.message}")
            }
        }
    }

    private fun updateConnectionStatus() {
        scope.launch {
            try {
                val isConnected = withContext(Dispatchers.IO) {
                    printer.isConnected()
                }
                binding.tvStatus.text = if (isConnected) "Status: Connected" else "Status: Disconnected"
                binding.tvStatus.setTextColor(
                    if (isConnected) getColor(android.R.color.holo_green_dark)
                    else getColor(android.R.color.holo_red_dark)
                )
            } catch (e: Exception) {
                Log.e("MainActivity", "Error updating status: ${e.message}")
            }
        }
    }

    private fun showToast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    private fun hexStringToByteArray(hex: String): ByteArray {
        val cleanHex = hex.replace(" ", "").replace("0x", "").replace(",", "")
        require(cleanHex.length % 2 == 0) { "Invalid hex string length" }

        return ByteArray(cleanHex.length / 2) { i ->
            val index = i * 2
            cleanHex.substring(index, index + 2).toInt(16).toByte()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        printer.close()
    }
}