package com.example.lpstudio.ui.fragments

import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment

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