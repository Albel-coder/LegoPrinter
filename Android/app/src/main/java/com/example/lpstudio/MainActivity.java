package com.example.lpstudio;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;
import java.util.HashMap;
import java.util.Map;

public class MainActivity extends AppCompatActivity {

    private FrameLayout contentContainer;
    private Map<String, Fragment> tabs = new HashMap<>();
    private Fragment currentFragment;

    // Button links
    private Button buttonDevice, buttonPreview, buttonPrepare, buttonCalibration;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        contentContainer = findViewById(R.id.contentContainer);

        // Initializing the buttons
        buttonDevice = findViewById(R.id.buttonDevice);
        buttonPreview = findViewById(R.id.buttonPreview);
        buttonPrepare = findViewById(R.id.buttonPrepare);
        buttonCalibration = findViewById(R.id.buttonCalibration);

        // Initializing tabs
        setupTabs();

        // Assigning click handlers
        buttonDevice.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                showTab("DeviceUserControl");
            }
        });

        buttonPreview.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                showTab("PreviewUserControl");
            }
        });

        buttonPrepare.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                showTab("PrepareUserControl");
            }
        });

        buttonCalibration.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                showTab("CalibrationUserControl");
            }
        });

        // Showing the start tab
        showTab("DeviceUserControl");
    }

    private void setupTabs() {
        // Here we will create instances of Kotlin fragments
        tabs.put("DeviceUserControl", createPlaceholderFragment("Device Tab Content", R.color.teal_200));
        tabs.put("PreviewUserControl", createPlaceholderFragment("Preview Tab Content", R.color.purple_500));
        tabs.put("PrepareUserControl", createPlaceholderFragment("Prepare Tab Content", R.color.teal_700));
        tabs.put("CalibrationUserControl", createPlaceholderFragment("Calibration Tab Content", R.color. purple_700));
    }

    private Fragment createPlaceholderFragment(String text, int colorResId) {
        PlaceholderFragment fragment = new PlaceholderFragment();
        Bundle args = new Bundle();
        args.putString("text", text);
        args.putInt("color", colorResId);
        fragment.setArguments(args);
        return fragment;
    }

    private void showTab(String tabKey) {
        Fragment fragment = tabs.get(tabKey);
        if (fragment == null) return;

        // Updating the state of the buttons
        updateButtonStates(tabKey);

        // We begin the transaction of fragments
        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();

        // If the fragment has not been added yet, replace the current one
        if (currentFragment != null) {
            transaction.hide(currentFragment);
        }

        // If the fragment has already been added, just show it
        if (!fragment.isAdded()) {
            transaction.add(R.id.contentContainer, fragment, tabKey);
        }
        else {
            transaction.show(fragment);
        }

        transaction.commit();
        currentFragment = fragment;
    }

    private void updateButtonStates(String selectedTab) {
        // Clear the selection for all buttons
        setButtonSelected(buttonDevice, false);
        setButtonSelected(buttonPreview, false);
        setButtonSelected(buttonPrepare, false);
        setButtonSelected(buttonCalibration, false);

        // Select the selected button
        switch(selectedTab) {
            case "DeviceUserControl":
                setButtonSelected(buttonDevice, true);
                break;
            case "PreviewUserControl":
                setButtonSelected(buttonPreview, true);
                break;
            case "PrepareUserControl":
                setButtonSelected(buttonPrepare, true);
                break;
            case "CalibrationUserControl":
                setButtonSelected(buttonCalibration, true);
                break;
        }
    }

    private void setButtonSelected(Button button, boolean selected) {

        // Change the button color depending on its state
        ImageView icon = null;
        TextView text = null;

        // Finding the child elements of a button
        for (int i = 0; i < ((android.view.ViewGroup) button).getChildCount(); i++) {
            View child = ((android.view.ViewGroup) button).getChildAt(i);
            if (child instanceof ImageView) {
                icon = (ImageView) child;
            }
            else if (child instanceof TextView) {
                text = (TextView) child;
            }
        }

        if(selected) {
            // Selected state
            if (icon != null) {
                // Change the icon to a filled version
                switch (button.getId()) {
                    case R.id.buttonDevice:
                        icon.setImageResource(R.drawable.ic_device_filled);
                        break;
                    case R.id.buttonPreview:
                        icon.setImageResource(R.drawable.ic_preview_filled);
                        break;
                    case R.id.buttonPrepare:
                        icon.setImageResource(R.drawable.ic_prepare_filled);
                        break;
                    case R.id.buttonCalibration:
                        icon.setImageResource(R.drawable.ic_calibration_filled);
                        break;
                }
            }
            if (text != null) {
                text.setTextColor(ContextCompat.getColor(this, R.color.bottom_nav_selected));
            }
        }
        else {
            // Not selected state
            if (icon != null) {
                // Change the icon to the outline version
                switch(button.getId()) {
                    case R.id.buttonDevice:
                        icon.setImageResource(R.drawable.ic_device_outline);
                        break;
                    case R.id.buttonPreview:
                        icon.setImageResource(R.drawable.ic_preview_outline);
                        break;
                    case R.id.buttonPrepare:
                        icon.setImageResource(R.drawable.ic_prepare_outline);
                        break;
                    case R.id.buttonCalibration:
                        icon.setImageResource(R.drawable.ic_calibration_outline);
                        break;
                }
            }
        }
    }
}
