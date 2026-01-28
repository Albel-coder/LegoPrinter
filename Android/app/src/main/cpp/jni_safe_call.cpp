// jni_safe_call.cpp
#include "jni_safe_call.h"
#include "printer_jni.h"

// Инициализация JVM в JNI_OnLoad
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("JNI_OnLoad OK - JVM initialized, g_jvm=%p", g_jvm);
    
    // Получаем JNIEnv для текущего потока
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNIEnv in JNI_OnLoad");
        return JNI_ERR;
    }
    
    // Кеширование Java классов при загрузке
    jclass localSpeedProfileClass = env->FindClass("com/example/lpstudio/PrinterController$SpeedProfile");
    if (localSpeedProfileClass) {
        g_SpeedProfileClass = static_cast<jclass>(env->NewGlobalRef(localSpeedProfileClass));
        env->DeleteLocalRef(localSpeedProfileClass);
        LOGI("SpeedProfile class cached successfully");
    } else {
        LOGE("Failed to find SpeedProfile class");
    }
    
    jclass localSpeedProfilePointClass = env->FindClass("com/example/lpstudio/PrinterController$SpeedProfilePoint");
    if (localSpeedProfilePointClass) {
        g_SpeedProfilePointClass = static_cast<jclass>(env->NewGlobalRef(localSpeedProfilePointClass));
        env->DeleteLocalRef(localSpeedProfilePointClass);
        LOGI("SpeedProfilePoint class cached successfully");
    } else {
        LOGE("Failed to find SpeedProfilePoint class");
    }
    
    return JNI_VERSION_1_6;
}

// ========== NativeLib JNI Methods ==========

extern "C"
JNIEXPORT void JNICALL
Java_com_example_lpstudio_NativeLib_nativeInit(JNIEnv* env, jclass clazz) {
    // Эта функция вызывается из Java для инициализации
    LOGI("NativeLib.nativeInit() called, g_jvm=%p", g_jvm);
    
    // Можно выполнить дополнительную инициализацию здесь
    if (g_SpeedProfileClass && g_SpeedProfilePointClass) {
        LOGI("Java classes are properly cached");
    } else {
        LOGE("Failed to cache Java classes!");
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_NativeLib_isJNIInitialized(JNIEnv* env, jclass clazz) {
    LOGI("NativeLib.isJNIInitialized() called, g_jvm=%p", g_jvm);
    return (g_jvm != nullptr) ? JNI_TRUE : JNI_FALSE;
}

JNISafeCall::JNISafeCall() : jvm(g_jvm), env_(nullptr), attached(false) {
    LOGI("JNISafeCall constructor, g_jvm=%p", g_jvm);
    
    if (!jvm) {
        LOGE("JavaVM is not initialized!");
        return;
    }
    
    // Пытаемся получить существующий JNIEnv
    jint result = jvm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
    
    if (result == JNI_EDETACHED) {
        // Поток не прикреплен - прикрепляем
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_6;
        args.name = "NativeThread";
        args.group = nullptr;
        
        if (jvm->AttachCurrentThread(&env_, &args) == JNI_OK) {
            attached = true;
            LOGI("Thread attached to JVM");
        } else {
            LOGE("Failed to attach thread to JVM!");
            env_ = nullptr;
        }
    } else if (result == JNI_OK) {
        // Поток уже прикреплен
        LOGI("Thread already attached to JVM");
    } else {
        LOGE("Failed to get JNIEnv: error code %d", result);
        env_ = nullptr;
    }
}

JNISafeCall::~JNISafeCall() {
    if (attached && jvm && env_) {
        jvm->DetachCurrentThread();
        LOGI("Thread detached from JVM");
    }
}
