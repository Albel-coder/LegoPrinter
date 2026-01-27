// jni_bridge.cpp
#include <jni.h>
#include "printer_jni.h"
#include "jni_safe_call.h"

extern "C" {

// ========== NativeLib дополнительные функции ==========

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_NativeLib_getLibraryVersion(JNIEnv* env, jclass clazz) {
    JNISafeCall guard;
    if (!guard.valid()) {
        LOGE("JNI not initialized in getLibraryVersion");
        return env->NewStringUTF("JNI not initialized");
    }
    
    const char* version = "Printer JNI v2.1 (SafeCall Architecture)";
    return guard.env()->NewStringUTF(version);
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_NativeLib_getRuntimeInfo(JNIEnv* env, jclass clazz) {
    JNISafeCall guard;
    if (!guard.valid()) {
        LOGE("JNI not initialized in getRuntimeInfo");
        return env->NewStringUTF("JNI not initialized");
    }
    
    char info[256];
    snprintf(info, sizeof(info), 
             "JNI Runtime Info:\n"
             "- JVM: %s (g_jvm=%p)\n"
             "- SpeedProfileClass cached: %s\n"
             "- SpeedProfilePointClass cached: %s",
             g_jvm ? "Initialized" : "Not initialized", g_jvm,
             g_SpeedProfileClass ? "Yes" : "No",
             g_SpeedProfilePointClass ? "Yes" : "No");
    
    return guard.env()->NewStringUTF(info);
}

} // extern "C"
