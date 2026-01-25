#include <jni.h>
#include <android/log.h>

#define LOG_TAG "JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Глобальная переменная JavaVM
static JavaVM* g_jvm = nullptr;

// Простая функция для получения JNIEnv
extern "C" JNIEnv* GetJNIEnv() {  // Добавлен extern "C"
    if (!g_jvm) {
        LOGE("GetJNIEnv: g_jvm is null!");
        return nullptr;
    }
    
    JNIEnv* env = nullptr;
    jint result = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    
    if (result == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("Failed to attach thread to JVM");
            return nullptr;
        }
    } else if (result != JNI_OK) {
        LOGE("Failed to get JNIEnv: %d", result);
        return nullptr;
    }
    
    return env;
}

// Функция для проверки инициализации JNI (используется в C++ коде)
extern "C" bool IsJNIInitialized() {  // Добавлен extern "C"
    return g_jvm != nullptr;
}

// Макрос для подавления предупреждений о неиспользуемых параметрах
#define UNUSED(x) (void)(x)

extern "C" {

// Основная функция загрузки
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("=== JNI_OnLoad called ===");
    
    g_jvm = vm;
    LOGI("JVM saved: %p", vm);
    
    UNUSED(reserved);
    return JNI_VERSION_1_6;
}

// Функция для проверки инициализации (Java-вызов)
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_NativeLib_isJNIInitialized(JNIEnv* env, jclass clazz) {
    UNUSED(env);
    UNUSED(clazz);
    return (g_jvm != nullptr) ? JNI_TRUE : JNI_FALSE;
}

// Дополнительная функция для получения JNIEnv из Java
JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_NativeLib_getJVM(JNIEnv* env, jclass clazz) {
    UNUSED(env);
    UNUSED(clazz);
    return reinterpret_cast<jlong>(g_jvm);
}

} // extern "C"
