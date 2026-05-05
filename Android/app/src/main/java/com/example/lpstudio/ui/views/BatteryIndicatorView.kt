package com.example.lpstudio.ui.views

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.util.AttributeSet
import android.view.View
import com.example.lpstudio.printer.PrinterController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class BatteryIndicatorView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private var batteryLevel: Int = 0
    private var isConnected: Boolean = false
    private var printerController: PrinterController? = null

    // Paint objects
    private val batteryPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        color = Color.WHITE
    }

    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = 24f
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        Paint.setTypeface = Typeface.DEFAULT_BOLD
    }

    // Dimensions
    private val batteryWidth = 80f
    private val batteryHeight = 40f
    private val batteryCapWidth = 10f
    private val batteryCapHeight = 20f
    private val padding = 4f

    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private var updateJob: Job? = null

    fun setPrinterController(controller: PrinterController?) {
        printerController = controller
        if (controller != null && controller.isConnected) {
            startMonitoring()
        } else {
            stopMonitoring()
            batteryLevel = 0
            isConnected = false
            invalidate()
        }
    }

    fun updateBatteryLevel(level: Int, connected: Boolean) {
        if (this.batteryLevel != level || this.isConnected != connected) {
            this.batteryLevel = level.coerceIn(0, 100)
            this.isConnected = connected
            invalidate()
        }
    }

    private fun startMonitoring() {
        updateJob?.cancel()
        updateJob = scope.launch {
            while (isActive) {
                updateBattery()
                delay(5000L) // Update every 5 seconds
            }
        }
    }

    private suspend fun updateBattery() {
        val controller = printerController ?: return

        if (!controller.isConnected) {
            withContext(Dispatchers.Main) {
                isConnected = false
                batteryLevel = 0
                invalidate()
            }
            return
        }
    }

    private fun stopMonitoring() {
        updateJob?.cancel()
        updateJob = null
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val centerX = width / 2f
        val centerY = height / 2f

        // Draw the battery case
        val batteryLeft = centerX - batteryWidth / 2
        val batteryTop = centerY - batteryHeight / 2
        val batteryRight = batteryLeft + batteryWidth
        val batteryBottom = batteryTop + batteryHeight

        // Main battery rectangle
        canvas.drawRect(
            batteryLeft, batteryTop,
            batteryRight, batteryBottom,
            batteryPaint
        )

        // Battery cap
        val capLeft = batteryRight
        val capTop = centerY - batteryCapHeight / 2
        val capBottom = capTop + batteryCapHeight

        canvas.drawRect(
            capLeft, capTop,
            capLeft + batteryCapWidth, capBottom,
            batteryPaint
        )

        if (isConnected && batteryLevel > 0) {
            // Filling the battery
            val fillColor = getBatteryColor()
            fillPaint.color = fillColor

            val fillWidth = (batteryWidth - 2 * padding) * batteryLevel / 100f
            val fillLeft = batteryLeft + padding
            val fillRight = fillLeft + fillWidth.coerceAtLeast(1f)

            canvas.drawRect(
                fillLeft, batteryTop + padding,
                fillRight, batteryBottom - padding,
                fillPaint
            )

            // Text with percentage
            val text = "$batteryLevel%"
            val textY = centerY - (textPaint.descent() + textPaint.ascent()) / 2

            canvas.drawText(
                text,
                centerX,
                textY,
                textPaint
            )
        } else {
            // State disabled
            fillPaint.color = Color.GRAY
            canvas.drawRect(
                batteryLeft + padding, batteryTop + padding,
                batteryRight - padding, batteryBottom - padding,
                fillPaint
            )

            // Cross
            val crossSize = 15f
            val crossPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.DKGRAY
                strokeWidth = 3f
            }

            canvas.drawLine(
                centerX - crossSize, centerY - crossSize,
                centerX + crossSize, centerY + crossSize,
                crossPaint
            )

            canvas.drawLine(
                centerX + crossSize, centerY - crossSize,
                centerX - crossSize, centerY + crossSize,
                crossPaint
            )
        }
    }

    private fun getBatteryColor(): Int {
        return when {
            batteryLevel < 15 -> Color.RED
            batteryLevel < 30 -> Color.YELLOW
            batteryLevel < 50 -> Color.parseColor("#FFA500") // Orange
            else -> Color.GREEN
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        stopMonitoring()
        scope.cancel()
    }
}