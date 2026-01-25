// include/jni_util.h
#pragma once

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "PrinterJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

// Объявляем глобальную переменную JavaVM
extern JavaVM* g_jvm;

// Объявляем функции
JNIEnv* GetJNIEnv();
bool IsJNIInitialized();

#ifdef __cplusplus
}
#endif
