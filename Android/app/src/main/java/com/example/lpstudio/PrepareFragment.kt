package com.example.lpstudio

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri
import android.os.*
import android.provider.Settings
import android.view.Gravity
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

class PrepareFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedIndicatorView: Bundle?
    ): View? {
        return TextView(requireContext()).apply {
            text = "Prepare Fragment"
            gravity = Gravity.CENTER
        }
    }
}