package com.example.lpstudio

import android.Manifest
import android.annotation.SuppressLint
import android.app.AlertDialog
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.*
import android.provider.Settings
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import kotlinx.coroutines.*
import java.io.File
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.atomic.AtomicBoolean

class DeviceFragment : Fragment() {

    // UI elements
    private lateinit var connectButton: Button
    private lateinit var batteryIndicator: BatteryIndicatorView
    private lateinit var consoleTextView: TextView
    private lateinit var consoleScrollView: ScrollView
    private lateinit var filePathTextView: TextView
    private lateinit var executeButton: Button
    private lateinit var browseButton: Button

    // Control buttons
    private lateinit var moveXLeftButton: Button
    private lateinit var moveXRightButton: Button
    private lateinit var moveYUpButton: Button
    private lateinit var moveYDownButton: Button
    private lateinit var moveZUpButton: Button
    private lateinit var moveZDownButton: Button

    // Home buttons
    private lateinit var homeAllButton: Button
    private lateinit var homeXYButton: Button
    private lateinit var homeXButton: Button
    private lateinit var homeYButton: Button
    private lateinit var homeZButton: Button

    // PrinterController in Java
    private lateinit var printerController: PrinterController
    private lateinit var gCodeInterpreter: GCodeInterpreter

    private val handler = Handler(Looper.getMainLooper())
    private var isConnected = false
    private var isConsoleUpdating = AtomicBoolean(false)
    private var selectedFile: File? = null

    // Log tracking
    private var lastDriverLogCount = 0
    private var lastInterpreterLogCount = 0

    // Coroutine scope
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    // Auto scroll
    private var autoScrollEnabled = true

    // Timer for logs
    private val logTimer = object : CountDownTimer(Long.MAX_VALUE, 500) { // Увеличили интервал до 500мс
        override fun onTick(millisUntilFinished: Long) {
            updateConsoleDisplay()
        }

        override fun onFinish() {}
    }

    // Bluetooth adapter
    private lateinit var bluetoothAdapter: BluetoothAdapter

    // Permissions for Bluetooth
    private val blePermissions: Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        }
        else {
            arrayOf(
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        }

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
            val allGranted = result.all { it.value }
            if (allGranted) {
                appendToConsole("[INFO] Разрешения Bluetooth получены")
                // After receiving permissions, we check if Bluetooth is enabled.
                checkBluetoothEnabled()
            } else {
                appendToConsole("[WARN] Некоторые разрешения Bluetooth отклонены")
                // Show a dialog with an explanation
                showPermissionExplanationDialog()
            }
        }
    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let {
            try {
                // Get the real path to the file
                val file = getFileFromUri(requireContext(), uri)
                selectedFile = file
                filePathTextView.text = file?.absolutePath ?: "Файл выбран"

                if (file != null) {
                    appendToConsole("[INFO] Выбран файл: ${file.name}")
                }
            } catch (e: Exception) {
                Toast.makeText(requireContext(), "Ошибка выбора файла", Toast.LENGTH_SHORT).show()
                appendToConsole("[ERROR] Ошибка выбора файла: ${e.message}")
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_device, container, false)
    }

    @SuppressLint("ClickableViewAccessibility", "MissingPermission")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Initialize your UI elements here
        connectButton = view.findViewById(R.id.connectButton)
        batteryIndicator = view.findViewById(R.id.batteryIndicator)
        consoleTextView = view.findViewById(R.id.consoleTextView)
        consoleScrollView = view.findViewById(R.id.consoleScrollView)
        filePathTextView = view.findViewById(R.id.filePathTextView)
        executeButton = view.findViewById(R.id.executeButton)
        browseButton = view.findViewById(R.id.browseButton)

        // Movement buttons
        moveXLeftButton = view.findViewById(R.id.moveXLeftButton)
        moveXRightButton = view.findViewById(R.id.moveXRightButton)
        moveYUpButton = view.findViewById(R.id.moveYUpButton)
        moveYDownButton = view.findViewById(R.id.moveYDownButton)
        moveZUpButton = view.findViewById(R.id.moveZUpButton)
        moveZDownButton = view.findViewById(R.id.moveZDownButton)

        // Home buttons
        homeAllButton = view.findViewById(R.id.homeAllButton)
        homeXYButton = view.findViewById(R.id.homeXYButton)
        homeXButton = view.findViewById(R.id.homeXButton)
        homeYButton = view.findViewById(R.id.homeYButton)
        homeZButton = view.findViewById(R.id.homeZButton)

        // Initialize on a background thread to avoid blocking the UI
        scope.launch(Dispatchers.IO) {
            try {
                // Initialize controllers
                printerController = PrinterController(context)
                gCodeInterpreter = GCodeInterpreter(printerController)

                withContext(Dispatchers.Main) {
                    // Setup button listeners
                    setupButtonListeners()

                    // Setup console touch listener for scroll control
                    consoleTextView.setOnTouchListener { v, event ->
                        when (event.action) {
                            MotionEvent.ACTION_DOWN -> {
                                autoScrollEnabled = false
                            }
                            MotionEvent.ACTION_UP -> {
                                autoScrollEnabled = true
                            }
                        }
                        false
                    }

                    // Start log timer
                    logTimer.start()

                    // Initialize Bluetooth
                    initBluetooth()

                    // Check permissions
                    checkAndRequestPermissions()

                    // Print information about initialization
                    appendToConsole("[INFO] Приложение инициализировано")
                    appendToConsole("[INFO] Контроллеры загружены успешно")
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    appendToConsole("[ERROR] Ошибка инициализации: ${e.message}")
                    Toast.makeText(requireContext(), "Ошибка инициализации", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    private fun setupButtonListeners() {
        connectButton.setOnClickListener {
            connectButtonClick()
        }

        browseButton.setOnClickListener {
            browseButtonClick()
        }

        executeButton.setOnClickListener {
            executeButtonClick()
        }

        // Movement buttons
        moveXLeftButton.setOnClickListener { sendGCodeCommand("G91\nG1 X-10 F1000") }
        moveXRightButton.setOnClickListener { sendGCodeCommand("G91\nG1 X10 F1000") }
        moveYUpButton.setOnClickListener { sendGCodeCommand("G91\nG1 Y10 F1000") }
        moveYDownButton.setOnClickListener { sendGCodeCommand("G91\nG1 Y-10 F1000") }
        moveZUpButton.setOnClickListener { sendGCodeCommand("G91\nG1 Z10 F1000") }
        moveZDownButton.setOnClickListener { sendGCodeCommand("G91\nG1 Z-10 F1000") }

        // Home buttons
        homeAllButton.setOnClickListener { sendGCodeCommand("G28") }
        homeXYButton.setOnClickListener { sendGCodeCommand("G28 X Y") }
        homeXButton.setOnClickListener { sendGCodeCommand("G28 X") }
        homeYButton.setOnClickListener { sendGCodeCommand("G28 Y") }
        homeZButton.setOnClickListener { sendGCodeCommand("G28 Z") }
    }

    private fun initBluetooth() {
        try {
            val bluetoothManager = requireContext().getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            bluetoothAdapter = bluetoothManager.adapter

            if (bluetoothAdapter == null) {
                appendToConsole("[ERROR] Устройство не поддерживает Bluetooth")
            } else {
                appendToConsole("[INFO] Bluetooth адаптер обнаружен")
            }
        } catch (e: Exception) {
            appendToConsole("[ERROR] Ошибка инициализации Bluetooth: ${e.message}")
        }
    }

    private fun checkAndRequestPermissions() {
        if (!hasBlePermissions()) {
            showPermissionRequestDialog()
        } else {
            // Check if Bluetooth is enabled
            checkBluetoothEnabled()
        }
    }

    private fun showPermissionRequestDialog() {
        val builder = AlertDialog.Builder(requireContext())
            .setTitle("Требуются разрешения")
            .setMessage("Для работы с Bluetooth принтером необходимы следующие разрешения:\n\n" +
                    "• Bluetooth подключение\n" +
                    "• Поиск устройств Bluetooth\n" +
                    "• Доступ к местоположению (для поиска устройств)\n\n" +
                    "Эти разрешения необходимы для поиска и подключения к вашему принтеру.")
            .setPositiveButton("Предоставить разрешения") { dialog, _ ->
                dialog.dismiss()
                permissionLauncher.launch(blePermissions)
            }
            .setNegativeButton("Отказаться") { dialog, _ ->
                dialog.dismiss()
                appendToConsole("[WARN] Работа с принтером невозможна без разрешений")
            }
            .setCancelable(false)

        builder.show()
    }

    private fun showPermissionExplanationDialog() {
        val builder = AlertDialog.Builder(requireContext())
            .setTitle("Разрешения не предоставлены")
            .setMessage("Для работы приложения необходимы разрешения Bluetooth. Вы можете:\n\n" +
                    "1. Нажать 'Запросить снова' для повторного запроса\n" +
                    "2. Нажать 'Настройки' для ручного предоставления разрешений\n" +
                    "3. Продолжить без Bluetooth (ограниченный функционал)")
            .setPositiveButton("Запросить снова") { dialog, _ ->
                dialog.dismiss()
                permissionLauncher.launch(blePermissions)
            }
            .setNeutralButton("Настройки") { dialog, _ ->
                dialog.dismiss()
                openAppSettings()
            }
            .setNegativeButton("Продолжить") { dialog, _ ->
                dialog.dismiss()
                appendToConsole("[INFO] Продолжение без Bluetooth разрешений")
            }

        builder.show()
    }

    private fun checkBluetoothEnabled() {
        if (bluetoothAdapter != null && !bluetoothAdapter.isEnabled) {
            val builder = AlertDialog.Builder(requireContext())
                .setTitle("Bluetooth отключен")
                .setMessage("Для работы с принтером необходимо включить Bluetooth")
                .setPositiveButton("Включить") { dialog, _ ->
                    dialog.dismiss()
                    val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                    startActivity(enableBtIntent)
                }
                .setNegativeButton("Отмена") { dialog, _ ->
                    dialog.dismiss()
                    appendToConsole("[WARN] Bluetooth отключен - подключение невозможно")
                }

            builder.show()
        }
    }

    private fun connectButtonClick() {
        scope.launch {
            // Checking Bluetooth readiness
            if (!ensureBleReady()) return@launch

            if (!isConnected) {
                // === CONNECT ===
                connectButton.isEnabled = false
                connectButton.text = "Подключение..."
                connectButton.setBackgroundColor(
                    ContextCompat.getColor(requireContext(), R.color.connecting)
                )

                try {
                    val success = withContext(Dispatchers.IO) {
                        printerController.connect()
                    }

                    if (!success) {
                        throw Exception("Connection failed")
                    }

                    isConnected = true

                    batteryIndicator.setPrinterController(printerController)
                    appendToConsole("[INFO] Принтер подключен")

                    connectButton.isEnabled = true
                    connectButton.text = "Отключить"
                    connectButton.setBackgroundColor(
                        ContextCompat.getColor(requireContext(), R.color.disconnect)
                    )
                } catch (e: Exception) {
                    isConnected = false

                    connectButton.isEnabled = true
                    connectButton.text = "Подключить"
                    connectButton.setBackgroundColor(
                        ContextCompat.getColor(requireContext(), R.color.connect)
                    )

                    appendToConsole("[ERROR] Ошибка подключения: ${e.message}")
                }
            } else {
                // === DISCONNECT ===
                connectButton.isEnabled = false
                connectButton.text = "Отключение..."
                connectButton.setBackgroundColor(
                    ContextCompat.getColor(requireContext(), R.color.connecting)
                )

                try {
                    if (gCodeInterpreter.isRunning()) {
                        gCodeInterpreter.pause()
                        delay(500)
                    }

                    val success = withContext(Dispatchers.IO) {
                        printerController.disconnect()
                    }

                    if (!success) {
                        throw Exception("Disconnect failed")
                    }

                    isConnected = false
                    batteryIndicator.setPrinterController(null)
                    appendToConsole("[INFO] Принтер отключен")

                    connectButton.isEnabled = true
                    connectButton.text = "Подключить"
                    connectButton.setBackgroundColor(
                        ContextCompat.getColor(requireContext(), R.color.connect)
                    )
                } catch (e: Exception) {
                    connectButton.isEnabled = true
                    connectButton.text = "Отключить"
                    connectButton.setBackgroundColor(
                        ContextCompat.getColor(requireContext(), R.color.disconnect)
                    )

                    appendToConsole("[ERROR] Ошибка отключения: ${e.message}")
                }
            }
        }
    }

    private fun browseButtonClick() {
        filePickerLauncher.launch("*/*")
    }

    private fun executeButtonClick() {
        scope.launch {
            // Checking the connection
            if (!isConnected) {
                appendToConsole("[ERROR] Сначала подключитесь к принтеру")
                return@launch
            }

            // Check if a file is selected
            if (selectedFile == null) {
                appendToConsole("[ERROR] Выберите файл G-code")
                return@launch
            }

            // Update the UI
            executeButton.isEnabled = false
            executeButton.text = "Выполнение..."

            try {
                val success = withContext(Dispatchers.IO) {
                    gCodeInterpreter.executeFile(selectedFile!!.absolutePath, printerController)
                }

                if (success) {
                    appendToConsole("[INFO] Выполнение G-code начато")
                    // Start execution monitoring
                    startExecutionMonitoring()
                } else {
                    val error = gCodeInterpreter.lastError
                    appendToConsole("[ERROR] Ошибка выполнения: $error")
                }
            } catch (e: Exception) {
                appendToConsole("[ERROR] Ошибка при выполнении: ${e.message}")
            } finally {
                executeButton.isEnabled = true
                executeButton.text = "Выполнить код"
            }
        }
    }

    private fun startExecutionMonitoring() {
        scope.launch {
            while (isActive && gCodeInterpreter.isRunning()) {
                // Updating progress
                val progress = gCodeInterpreter.progress
                val status = gCodeInterpreter.status

                withContext(Dispatchers.Main) {
                    executeButton.text = "Выполняется ${String.format("%.1f", progress * 100)}%"
                }

                // Check for completion
                if (status == GCodeInterpreter.Status.COMPLETED) {
                    withContext(Dispatchers.Main) {
                        executeButton.text = "Завершено"
                        appendToConsole("[INFO] Выполнение завершено")
                    }
                    break
                } else if (status == GCodeInterpreter.Status.ERROR) {
                    withContext(Dispatchers.Main) {
                        executeButton.text = "Ошибка"
                        val error = gCodeInterpreter.lastError
                        appendToConsole("[ERROR] Ошибка выполнения: $error")
                    }
                    break
                }

                delay(500)
            }
        }
    }

    private fun sendGCodeCommand(command: String) {
        scope.launch {
            if (!isConnected) {
                appendToConsole("[ERROR] Сначала подключитесь к принтеру")
                return@launch
            }

            try {
                val success = withContext(Dispatchers.IO) {
                    gCodeInterpreter.executeLine(command, printerController)
                }

                if (success) {
                    appendToConsole("[INFO] Команда выполнена: $command")
                } else {
                    val error = gCodeInterpreter.lastError
                    appendToConsole("[ERROR] Ошибка команды: $error")
                }
            } catch (e: Exception) {
                appendToConsole("[ERROR] Ошибка выполнения команды: ${e.message}")
            }
        }
    }

    private fun sendHomeCommand(axis: String) {
        val command = when (axis) {
            "X" -> "G28 X"
            "Y" -> "G28 Y"
            "Z" -> "G28 Z"
            "XY" -> "G28 X Y"
            "ALL" -> "G28"
            else -> "G28"
        }
        sendGCodeCommand(command)
    }

    private fun updateConsoleDisplay() {
        if (isConsoleUpdating.get()) return

        if (!isConsoleUpdating.compareAndSet(false, true)) return

        try {
            updateConsoleDisplayInternal()
        } finally {
            isConsoleUpdating.set(false)
        }
    }

    private fun updateConsoleDisplayInternal() {
        try {
            // Update driver logs
            val currentDriverLogCount = printerController.logCount

            // If the logs were cleared in the driver
            if (currentDriverLogCount < lastDriverLogCount) {
                lastDriverLogCount = 0
                // We don't clear the console completely, as there may be other logs there.
            }

            if (currentDriverLogCount > lastDriverLogCount) {
                val newDriverLogs = currentDriverLogCount - lastDriverLogCount
                appendDriverLogs(lastDriverLogCount, newDriverLogs)
                lastDriverLogCount = currentDriverLogCount
            }
        } catch (e: Exception) {
            // Ignore console update errors
        }
    }

    private fun appendDriverLogs(startIndex: Int, count: Int) {
        if (count < 1) return

        val stringBuilder = StringBuilder()

        for (i in 0 until count) {
            try {
                val logEntry = printerController.getLogEntry(startIndex + i)
                if (logEntry != null && logEntry.isNotEmpty()) {
                    stringBuilder.appendLine(logEntry)
                }
            } catch (e: Exception) {
                // Ignore errors getting one record
            }
        }

        if (stringBuilder.isNotEmpty()) {
            appendToConsole(stringBuilder.toString())
        }
    }

    private fun appendToConsole(text: String) {
        handler.post {
            try {
                val scrollView = consoleScrollView
                val textView = consoleTextView

                val wasAtBottom = isAtBottom(scrollView)

                val finalText =
                    if (text.endsWith("\n")) text
                    else "$text\n"

                textView.append(finalText)

                if (autoScrollEnabled && wasAtBottom) {
                    scrollView.post {
                        scrollView.fullScroll(View.FOCUS_DOWN)
                    }
                }

                limitLogSize()
            } catch (_: Exception) {
            }
        }
    }

    private fun clearConsole() {
        handler.post {
            consoleTextView.text = ""
            // Reset the counters when clearing the console
            lastDriverLogCount = 0
            lastInterpreterLogCount = 0
        }
    }

    private fun limitLogSize() {
        val text = consoleTextView.text.toString()
        val lines = text.split("\n")

        val MAX_LINES = 2000
        val KEEP_LINES = 1500

        if (lines.size > MAX_LINES) {
            val removeCount = lines.size - KEEP_LINES
            if (removeCount > 0) {
                val newLines = lines.subList(removeCount, lines.size)
                consoleTextView.text = newLines.joinToString("\n")

                // Adjusting the counters
                lastDriverLogCount = maxOf(0, lastDriverLogCount - removeCount)
                lastInterpreterLogCount = maxOf(0, lastInterpreterLogCount - removeCount)
            }
        }
    }

    private fun isAtBottom(scrollView: ScrollView): Boolean {
        return try {
            val child = scrollView.getChildAt(0)
            val childHeight = child.height
            val scrollViewHeight = scrollView.height
            val scrollY = scrollView.scrollY

            childHeight - scrollViewHeight - scrollY <= 100 // 100px tolerance
        } catch (e: Exception) {
            true
        }
    }

    private fun ensureBleReady(): Boolean {
        if (!hasBlePermissions()) {
            showPermissionRequestDialog()
            return false
        }

        if (bluetoothAdapter == null) {
            appendToConsole("[ERROR] Устройство не поддерживает Bluetooth")
            return false
        }

        if (!bluetoothAdapter.isEnabled) {
            checkBluetoothEnabled()
            return false
        }

        return true
    }

    private fun hasBlePermissions() : Boolean {
        return blePermissions.all {
            ContextCompat.checkSelfPermission(
                requireContext(),
                it
            ) == PackageManager.PERMISSION_GRANTED
        }
    }
    private fun openAppSettings() {
        try {
            val intent = Intent(
                Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.fromParts("package", requireContext().packageName, null)
            )
            startActivity(intent)
        } catch (e: Exception) {
            Toast.makeText(requireContext(), "Не удалось открыть настройки", Toast.LENGTH_SHORT).show()
        }
    }

    private fun getFileFromUri(context: Context, uri: Uri): File? {
        return try {
            val inputStream = context.contentResolver.openInputStream(uri)
            val tempFile = File.createTempFile("gcode_", ".gcode", context.cacheDir)
            tempFile.outputStream().use { output ->
                inputStream?.copyTo(output)
            }
            tempFile
        } catch (e: Exception) {
            null
        }
    }

    fun clearLogs() {
        scope.launch(Dispatchers.IO) {
            try {
                printerController.clearLog()
            } catch (e: Exception) {
                // Ignore cleanup errors
            }
        }
        clearConsole()
    }

    override fun onDestroy() {
        super.onDestroy()
        logTimer.cancel()
        scope.cancel()

        try {
            printerController.close()
        } catch (e: Exception) {
            // Ignore cleanup errors
        }

        try {
            gCodeInterpreter.close()
        } catch (e: Exception) {
            // Ignore cleanup errors
        }
    }
}