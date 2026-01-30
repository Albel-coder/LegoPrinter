package com.example.lpstudio;

import android.util.Log;

public final class NativeLib {
    private static final String TAG = "NativeLib";
    private static boolean loaded = false;

    private NativeLib() {}

    private static native void nativeInit();

    // Статический метод для проверки инициализации JNI
    public static native boolean isJNIInitialized();

    // Получить версию библиотеки
    public static native String getLibraryVersion();

    // Получить информацию о рантайме
    public static native String getRuntimeInfo();

    public static synchronized void ensureLoaded() {
        if (!loaded) {
            System.loadLibrary("printer-jni");
            nativeInit();
            loaded = true;
            Log.i("NativeLib", "Library loaded and JNI initialized");
        }
    }
}
