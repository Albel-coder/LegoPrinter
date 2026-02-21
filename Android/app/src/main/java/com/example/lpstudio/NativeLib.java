package com.example.lpstudio;

public final class NativeLib {
    private static boolean loaded = false;
    private NativeLib() {}

    public static synchronized void ensureLoaded() {
        if (!loaded) {
            System.loadLibrary("printer-jni");
            loaded = true;
        }
    }
}
