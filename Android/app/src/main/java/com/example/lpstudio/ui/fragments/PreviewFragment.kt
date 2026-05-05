package com.example.lpstudio.ui.fragments

import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment

class PreviewFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedIndicatorView: Bundle?
    ): View? {
        return TextView(requireContext()).apply {
            text = "Preview Fragment"
            gravity = Gravity.CENTER
        }
    }
}