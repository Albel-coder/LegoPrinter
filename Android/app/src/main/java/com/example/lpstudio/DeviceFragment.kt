package com.example.lpstudio

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent;
import android.content.pm.PackageManager;
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
import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.reflect.KDeclarationContainer
import java.util.concurrent.atomic.AtomicBoolean

class DeviceFragment : Fragment() {

    // UI elements
    private lateinit var connectButton: Button
    private lateinit var batteryIndicator: BatteryIndicatorView
    private lateinit var consoleTextView: TextView
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

    private val handler = Handler(Looper.getMainLooper())
    private var isConnected = false
    private var isConsoleUpdating = false
    private var selectedFile: File? = null

    // Log tracking
    private var lastDriverLogCount = 0
    private var lastInterpreterLogCount = 0

    // Coroutine scope
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    // Auto scroll
    private var autoScrollEnabled = true

    // Timer for logs
    private val logTimer = object : CountDownTimer(Long.MAX_VALUE, 100) {
        override fun onTick(millisUntilFinished: Long) {
            // updateConsoleDisplay()
        }

        override fun onFinish() {}
    }

    // Permissions for Bluetooth
    private val permissions = arrayOf(
        Manifest.permission.BLUETOOTH,
        Manifest.permission.BLUETOOTH_ADMIN,
        Manifest.permission.ACCESS_FINE_LOCATION,
        Manifest.permission.READ_EXTERNAL_STORAGE
    )

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.all { it.value }) {
            Toast.makeText(requireContext(), "Разрешения предоставлены", Toast.LENGTH_SHORT).show()
        }
        else {
            Toast.makeText(requireContext(), "Необходимы разрешения для работы с Bluetooth", Toast.LENGTH_LONG).show()
            // openAppSettings()
        }
    }

    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let {
            val file = File(uri.path ?: "")
            selectedFile = file
            filePathTextView.text = file.absolutePath
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(R.layout.fragment_device, container, false)
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Initialize your UI elements here
        connectButton = view.findViewById(R.id.connectButton)
        batteryIndicator = view.findViewById(R.id.batteryIndicator)
        consoleTextView = view.findViewById(R.id.consoleTextView)
        //consoleScrollView = view.findViewById(R.id.consoleScrollView)
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

        // Initialize controllers
        printerController = PrinterController()
        // gCodeInterpreter = GCodeInterpreter()

        // Load printer config
        try {
            val configFile = File(requireContext().filesDir, "Printer.cfg")
            // gCodeInterpreter.readConfig(configFile.absolutePath)
        }
        catch (e: Exception) {
            // appendToConsole("[ERROR] Failed to load printer config: ${e.message}")
        }

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

        // Check permissions

        // Start log timer
        logTimer.start()
    }

    private fun setupButtonListeners() {
        connectButton.setOnClickListener {
            connectButtonClick()
        }
    }

    private fun connectButtonClick() {

    }

    private fun browseButtonClick() {

    }

    private fun executeButtonClick() {

    }

    private fun sendMoveCommand(direction: String) {

    }

    private fun sendHomeCommand(axis: String) {

    }

    private fun updateConsoleDisplay() {

    }

    private fun updateConsoleDisplayInternal() {

    }

    private fun appendDriverLogs(startIndex: Int, count: Int) {

    }

    private fun appendInterpreterLogs(startIndex: Int, count: Int) {

    }

    private fun appendToConsole(text: String) {

    }

    private fun clearConsole() {

    }

    private fun limitLogSize() {

    }

    private fun isAtBottom(scrollView: ScrollView): Boolean {
        return false
    }

    private fun hasPermissions(): Boolean {
        return false
    }

    private fun openAppSettings() {

    }

    fun clearLogs() {

    }

    override fun onDestroy() {
        super.onDestroy()
        logTimer.cancel()
        scope.cancel()
        printerController.close()
    }
}