package com.example.lpstudio.ui.fragments;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import com.example.lpstudio.R;

public class PlaceholderFragment extends Fragment {

    public PlaceholderFragment() {
        // Required empty constructor
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_placeholder, container, false);

        TextView textView = view.findViewById(R.id.textView);
        View background = view.findViewById(R.id.background);

        if (getArguments() != null) {
            String text = getArguments().getString("text");
            int colorResId = getArguments().getInt("color", R.color.teal_200);

            textView.setText(text);
            background.setBackgroundColor(getResources().getColor(colorResId));
        }

        return view;
    }
}