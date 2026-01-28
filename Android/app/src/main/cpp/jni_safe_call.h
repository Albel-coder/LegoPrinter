// jni_safe_call.h
#pragma once
#include <jni.h>
#include <android/log.h>
#include "jni_globals.h"  // Подключаем глобалы

#define LOG_TAG "JNISafeCall"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class JNISafeCall {
private:
    JavaVM* jvm;
    JNIEnv* env_;
    bool attached;
    
public:
    JNISafeCall();
    ~JNISafeCall();
    
    bool valid() const { return env_ != nullptr; }
    JNIEnv* env() const { return env_; }
    
    // Helper методы для работы со строками
    const char* getStringUTFChars(jstring str, jboolean* isCopy = nullptr) {
        if (!env_ || !str) return nullptr;
        return env_->GetStringUTFChars(str, isCopy);
    }
    
    void releaseStringUTFChars(jstring str, const char* chars) {
        if (env_ && str && chars) {
            env_->ReleaseStringUTFChars(str, chars);
        }
    }
    
    // Helper методы для работы с массивами
    jbyte* getByteArrayElements(jbyteArray array, jboolean* isCopy = nullptr) {
        if (!env_ || !array) return nullptr;
        return env_->GetByteArrayElements(array, isCopy);
    }
    
    void releaseByteArrayElements(jbyteArray array, jbyte* elems, jint mode = 0) {
        if (env_ && array && elems) {
            env_->ReleaseByteArrayElements(array, elems, mode);
        }
    }
    
    // Helper методы для работы с объектами
    jobject getObjectArrayElement(jobjectArray array, jsize index) {
        if (!env_ || !array) return nullptr;
        return env_->GetObjectArrayElement(array, index);
    }
    
    void deleteLocalRef(jobject obj) {
        if (env_ && obj) {
            env_->DeleteLocalRef(obj);
        }
    }
    
    jsize getArrayLength(jarray array) {
        if (!env_ || !array) return 0;
        return env_->GetArrayLength(array);
    }
};
