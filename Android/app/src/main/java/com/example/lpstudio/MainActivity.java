package com.example.lpstudio;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleEventObserver;
import androidx.lifecycle.LifecycleOwner;
import android.graphics.drawable.Drawable;
import android.app.AlertDialog;
import android.util.Log;

// Kotlin импорты
import com.example.lpstudio.coordinator.UpdateCoordinator;
import com.example.lpstudio.services.AndroidUpdateService;
import kotlin.Unit;
import kotlinx.coroutines.CoroutineScope;
import kotlinx.coroutines.flow.FlowKt;
import androidx.lifecycle.LifecycleOwnerKt;

import java.util.HashMap;
import java.util.Map;

public class MainActivity extends AppCompatActivity {

    private FrameLayout contentContainer;
    private Map<String, Fragment> tabs = new HashMap<>();
    private Fragment currentFragment;

    private Button buttonDevice, buttonPreview, buttonPrepare, buttonCalibration;

    // Update system
    private UpdateCoordinator updateCoordinator;
    private Handler updateHandler;

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

        // Initialize update system
        initUpdater();
    }

    // 🔥 НОВЫЙ МЕТОД - инициализация updater
    private void initUpdater() {
        updateCoordinator = new UpdateCoordinator(this);
        updateHandler = new Handler(Looper.getMainLooper());

        // Cleanup при уничтожении
        getLifecycle().addObserver(new LifecycleEventObserver() {
            @Override
            public void onStateChanged(LifecycleOwner source, Lifecycle.Event event) {
                if (event == Lifecycle.Event.ON_DESTROY) {
                    updateCoordinator.cleanup();
                }
            }
        });

        // Старт проверки через 3 секунды
        updateHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                updateCoordinator.checkForUpdates(false);
            }
        }, 3000);

        // Подписка на StateFlow из Kotlin
        LifecycleOwner owner = this;
        CoroutineScope scope = LifecycleOwnerKt.getLifecycleScope(owner);

        scope.launchWhenStarted(() -> {
            kotlinx.coroutines.flow.Flow<UpdateCoordinator.UpdateState> flow =
                    updateCoordinator.getUpdateState();

            return FlowKt.collect(flow, state -> {
                handleUpdateState(state);
                return Unit.INSTANCE;
            });
        });
    }

    // 🔥 Обработка состояний обновления
    private void handleUpdateState(UpdateCoordinator.UpdateState state) {
        if (state instanceof UpdateCoordinator.UpdateState.Available) {
            AndroidUpdateService.UpdateInfo info =
                    ((UpdateCoordinator.UpdateState.Available) state).getInfo();
            showUpdateDialog(info);
        }
        else if (state instanceof UpdateCoordinator.UpdateState.Error) {
            String error = ((UpdateCoordinator.UpdateState.Error) state).getMessage();
            Log.e("Updater", error);
        }
    }

    // 🔥 Показ диалога обновления
    private void showUpdateDialog(AndroidUpdateService.UpdateInfo info) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setTitle("Доступно обновление")
                .setMessage(
                        "Текущая версия: " + info.getCurrentVersion() + " (build " + info.getCurrentVersionCode() + ")\n" +
                                "Новая версия: " + info.getLatestVersion() + " (build " + info.getLatestVersionCode() + ")\n\n" +
                                info.getReleaseNotes()
                )
                .setPositiveButton("Обновить", (dialog, which) -> {
                    updateCoordinator.downloadUpdate(info);
                });

        if (!info.isRequired()) {
            builder.setNegativeButton("Позже", (dialog, which) -> {
                updateCoordinator.postponeUpdate(24);
            });

            builder.setNeutralButton("Пропустить", (dialog, which) -> {
                updateCoordinator.skipVersion(info.getLatestVersionCode());
            });
        }

        builder.setCancelable(!info.isRequired());
        builder.show();
    }

    @Override
    protected void onDestroy() {
        // Очистка ресурсов
        if (updateHandler != null) {
            updateHandler.removeCallbacksAndMessages(null);
        }
        super.onDestroy();
    }

    // 🔥 ВСЕ ОСТАЛЬНЫЕ МЕТОДЫ БЕЗ ИЗМЕНЕНИЙ
    private void showTab(String tabKey) {
        Fragment fragment = tabs.get(tabKey);
        if (fragment == null) return;

        updateButtonStates(tabKey);

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();

        if (currentFragment == null) {
            transaction.add(R.id.contentContainer, fragment, tabKey);
        } else if (currentFragment != fragment) {
            if (fragment.isAdded()) {
                transaction.hide(currentFragment).show(fragment);
            } else {
                transaction.hide(currentFragment).add(R.id.contentContainer, fragment, tabKey);
            }
        }

        currentFragment = fragment;
        transaction.commit();
    }

    private void updateButtonStates(String selectedTab) {
        setButtonSelected(buttonDevice, false);
        setButtonSelected(buttonPreview, false);
        setButtonSelected(buttonPrepare, false);
        setButtonSelected(buttonCalibration, false);

        if ("DeviceUserControl".equals(selectedTab)) {
            setButtonSelected(buttonDevice, true);
        } else if ("PreviewUserControl".equals(selectedTab)) {
            setButtonSelected(buttonPreview, true);
        } else if ("PrepareUserControl".equals(selectedTab)) {
            setButtonSelected(buttonPrepare, true);
        } else if ("CalibrationUserControl".equals(selectedTab)) {
            setButtonSelected(buttonCalibration, true);
        }
    }

    private void setButtonSelected(Button button, boolean selected) {
        button.setSelected(selected);

        if (selected) {
            button.setTextColor(ContextCompat.getColor(this, R.color.bottom_nav_selected));
        } else {
            button.setTextColor(ContextCompat.getColor(this, R.color.bottom_nav_unselected));
        }

        int iconResId = 0;
        int buttonId = button.getId();

        if (selected) {
            if (buttonId == R.id.buttonDevice) {
                iconResId = R.drawable.ic_device_filled;
            } else if (buttonId == R.id.buttonPreview) {
                iconResId = R.drawable.ic_preview_filled;
            } else if (buttonId == R.id.buttonPrepare) {
                iconResId = R.drawable.ic_prepare_filled;
            } else if (buttonId == R.id.buttonCalibration) {
                iconResId = R.drawable.ic_calibration_filled;
            }
        } else {
            if (buttonId == R.id.buttonDevice) {
                iconResId = R.drawable.ic_device_outline;
            } else if (buttonId == R.id.buttonPreview) {
                iconResId = R.drawable.ic_preview_outline;
            } else if (buttonId == R.id.buttonPrepare) {
                iconResId = R.drawable.ic_prepare_outline;
            } else if (buttonId == R.id.buttonCalibration) {
                iconResId = R.drawable.ic_calibration_outline;
            }
        }

        if (iconResId != 0) {
            Drawable icon = ContextCompat.getDrawable(this, iconResId);
            button.setCompoundDrawablesWithIntrinsicBounds(null, icon, null, null);
        }
    }
}
