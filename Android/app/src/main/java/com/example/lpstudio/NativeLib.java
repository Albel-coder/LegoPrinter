package com.example.lpstudio;

import android.util.Log;

public final class NativeLib {
    private static final String TAG = "NativeLib";

    private static boolean loaded = false;
    private static boolean jniInitialized = false;

    static {
        try {
            Log.i(TAG, "Attempting to load native library...");

            // Пытаемся загрузить библиотеку несколько раз
            for (int i = 0; i < 3; i++) {
                try {
                    System.loadLibrary("printer-jni");
                    loaded = true;
                    Log.i(TAG, "Native library loaded successfully on attempt " + (i + 1));
                    break;
                } catch (UnsatisfiedLinkError e) {
                    Log.w(TAG, "Attempt " + (i + 1) + " failed: " + e.getMessage());
                    if (i == 2) throw e;
                    try { Thread.sleep(100); } catch (InterruptedException ie) {}
                }
            }

            // Проверяем инициализацию JNI
            if (loaded) {
                jniInitialized = isJNIInitialized();
                Log.i(TAG, "JNI initialized: " + jniInitialized);

                if (!jniInitialized) {
                    // Даем еще один шанс
                    try { Thread.sleep(500); } catch (InterruptedException ie) {}
                    jniInitialized = isJNIInitialized();
                    Log.i(TAG, "JNI initialized after delay: " + jniInitialized);
                }

                if (!jniInitialized) {
                    throw new RuntimeException("JNI failed to initialize after loading library");
                }
            }

        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
            loaded = false;
            jniInitialized = false;
            throw new RuntimeException("Failed to load native library", e);
        } catch (Throwable t) {
            Log.e(TAG, "Unexpected error: " + t.getMessage(), t);
            loaded = false;
            jniInitialized = false;
            throw new RuntimeException("Native library initialization failed", t);
        }
    }

    // Нативные методы
    private static native boolean isJNIInitialized();
    private static native long getJVM();

    public static void ensureLoaded() {
        if (!loaded || !jniInitialized) {
            throw new IllegalStateException(
                    "Native library not properly loaded. Loaded: " + loaded +
                            ", JNI Initialized: " + jniInitialized
            );
        }
    }

    public static boolean isJNIReady() {
        return loaded && jniInitialized;
    }

    public static String getStatus() {
        return "Loaded: " + loaded + ", JNI Init: " + jniInitialized;
    }

    private NativeLib() {}
}
