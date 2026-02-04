package com.example.lpstudio;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;
import com.example.lpstudio.AndroidUpdateService;

public class UpdateCoordinator {
    private static final String TAG = "UpdateCoordinator";

    private final Context context;
    private final Handler mainHandler;
    private final AndroidUpdateService updateService;
    private UpdateListener listener;

    public UpdateCoordinator(Context context) {
        this(context, "Albel-coder", "LegoPrinter");
    }

    public UpdateCoordinator(Context context, String owner, String repo) {
        // Используем applicationContext для предотвращения утечек памяти
        this.context = context.getApplicationContext();
        this.mainHandler = new Handler(Looper.getMainLooper());
        this.updateService = new AndroidUpdateService(this.context, owner, repo);

        updateService.setUpdateCallback(new AndroidUpdateService.UpdateCallback() {
            @Override
            public void onError(String message) {
                mainHandler.post(() -> {
                    Toast.makeText(context, message, Toast.LENGTH_LONG).show();
                    if (listener != null) {
                        listener.onError(message);
                    }
                });
            }

            @Override
            public void onInfo(String message) {
                mainHandler.post(() -> {
                   Toast.makeText(context, message, Toast.LENGTH_LONG).show();
                });
            }
        });
    }

    public void setUpdateListener(UpdateListener listener) {
        this.listener = listener;
    }

    // Убираем лишний Thread - делегируем всю работу AndroidUpdateService
    public void checkForUpdates(boolean force) {
        updateService.checkForUpdates(new AndroidUpdateService.UpdateCheckCallback() {
            @Override
            public void onResult(AndroidUpdateService.UpdateInfo updateInfo) {
                mainHandler.post(() -> {
                    if (listener != null && updateInfo.isAvailable) {
                        listener.onUpdateAvailable(updateInfo);
                    }
                });
                Log.d("UpdateCheck", "Current: " + updateInfo.currentVersion + "/" + updateInfo.currentVersionCode +
                        ", Latest: " + updateInfo.latestVersion + "/" + updateInfo.latestBuild);
            }

            @Override
            public void onError(String error) {
                Log.e(TAG, "Update check error: " + error);
                mainHandler.post(() -> {
                    if (listener != null) {
                        listener.onError(error);
                    }
                });
            }
        });

    }

    public void downloadUpdate(AndroidUpdateService.UpdateInfo info) {
        // Оставляем Thread только для загрузки
        new Thread(() -> {
            try {
                long downloadId = updateService.downloadUpdate(info);
                Log.i(TAG, "Download started with ID: " + downloadId);

                mainHandler.post(() -> {
                    if (listener != null) {
                        listener.onDownloadStarted(info);
                    }
                });

            } catch (Exception e) {
                Log.e(TAG, "Download error: " + e.getMessage());
                mainHandler.post(() -> {
                    if (listener != null) {
                        listener.onError("Ошибка загрузки: " + e.getMessage());
                    }
                });
            }
        }).start();
    }

    public void skipVersion(int versionCode) {
        updateService.skipVersion(versionCode);
    }

    public void postponeUpdate(int hours) {
        updateService.postponeUpdate(hours);
    }

    public void cleanup() {
        updateService.cleanup();
    }

    public interface UpdateListener {
        void onUpdateAvailable(AndroidUpdateService.UpdateInfo info);
        void onDownloadStarted(AndroidUpdateService.UpdateInfo info);
        void onError(String message);
    }


}
