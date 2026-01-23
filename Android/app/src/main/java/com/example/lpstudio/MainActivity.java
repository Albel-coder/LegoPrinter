package com.example.lpstudio;

import com.example.lpstudio.DeviceFragment;
import com.example.lpstudio.PreviewFragment;
import android.graphics.drawable.Drawable;
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
        tabs.put("DeviceUserControl", new DeviceFragment());
        tabs.put("PreviewUserControl", new PreviewFragment());
        tabs.put("PrepareUserControl", new PrepareFragment());
        tabs.put("CalibrationUserControl", new CalibrationFragment());

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

    private void showTab(String tabKey) {
        Fragment fragment = tabs.get(tabKey);
        if (fragment == null) return;

        // Updating the state of the buttons
        updateButtonStates(tabKey);

        // We begin the transaction of fragments
        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();

        // If this is the first time showing a fragment
        if (currentFragment == null) {
            transaction.add(R.id.contentContainer, fragment, tabKey);
        }
        // If switching between fragments
        else if (currentFragment != fragment) {
            // If fragment was already added, just show it
            if (fragment.isAdded()) {
                transaction.hide(currentFragment).show(fragment);
            }
            // If fragment hasn`t been added yet
            else {
                transaction.hide(currentFragment).add(R.id.contentContainer, fragment, tabKey);
            }
        }

        currentFragment = fragment;
        transaction.commit();
    }

    private void updateButtonStates(String selectedTab) {
        // Clear the selection for all buttons
        setButtonSelected(buttonDevice, false);
        setButtonSelected(buttonPreview, false);
        setButtonSelected(buttonPrepare, false);
        setButtonSelected(buttonCalibration, false);

        // Select the selected button
        if ("DeviceUserControl".equals(selectedTab)) {
            setButtonSelected(buttonDevice, true);
        }
        else if ("PreviewUserControl".equals(selectedTab)) {
            setButtonSelected(buttonPreview, true);
        }
        else if ("PrepareUserControl".equals(selectedTab)) {
            setButtonSelected(buttonPrepare, true);
        }
        else if ("CalibrationUserControl".equals(selectedTab)) {
            setButtonSelected(buttonCalibration, true);
        }
    }

    private void setButtonSelected(Button button, boolean selected) {
        button.setSelected(selected);

        // Change the text color depending on the state
        if (selected) {
            button.setTextColor(ContextCompat.getColor(this, R.color.bottom_nav_selected));
        }
        else {
            button.setTextColor(ContextCompat.getColor(this, R.color.bottom_nav_unselected));
        }

        // Change the icon depending on the state and ID of the button
        int iconResId = 0;
        int buttonId = button.getId();

        // Use if-else instead of switch (to avoid the "Constant expression required" error)
        if (selected) {
            // Selected state - filled icons
            if (buttonId == R.id.buttonDevice) {
                iconResId = R.drawable.ic_device_filled;
            }
            else if (buttonId == R.id.buttonPreview) {
                iconResId = R.drawable.ic_preview_filled;
            }
            else if (buttonId == R.id.buttonPrepare) {
                iconResId = R.drawable.ic_prepare_filled;
            }
            else if (buttonId == R.id.buttonCalibration) {
                iconResId = R.drawable.ic_calibration_filled;
            }
        }
        else {
            // Unselected state - outline icons
            if (buttonId == R.id.buttonDevice) {
                iconResId = R.drawable.ic_device_outline;
            }
            else if (buttonId == R.id.buttonPreview) {
                iconResId = R.drawable.ic_preview_outline;
            }
            else if (buttonId == R.id.buttonPrepare) {
                iconResId = R.drawable.ic_prepare_outline;
            }
            else if (buttonId == R.id.buttonCalibration) {
                iconResId = R.drawable.ic_calibration_outline;
            }
        }

        // Installing the icon
        if (iconResId != 0) {
            Drawable icon = ContextCompat.getDrawable(this, iconResId);
            // Place the icon above the text
            button.setCompoundDrawablesWithIntrinsicBounds(null, icon, null, null);
        }
    }
}
