#pragma once

#include "ITransport.h"
#include <jni.h>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>

class TransportAndroid : public ITransport {
public:
    TransportAndroid(JavaVM* jvm, jobject context);
    ~TransportAndroid() override;

    // ITransport implementation
    bool open() override;
    void close() override;
    bool write(const uint8_t* data, size_t length) override;
    bool isConnected() override;

    void setDataCallback(std::function<void(const uint8_t*, size_t)> callback) override;
    void setConnectionCallback(std::function<void(bool)> callback) override;
    const char* getName() const override { return "AndroidBLE"; }

    // Methods called from Kotlin via JNI (callbacks)
    void onDataReceived(JNIEnv* env, jbyteArray data);
    void onConnectionStateChanged(JNIEnv* env, jboolean connected);
	void setDeviceAddress(const std::string& address);

private:
    JavaVM* jvm_;
    jobject bleManager_;        // global ref to Kotlin BleManager
    jmethodID openMethod_;
    jmethodID closeMethod_;
    jmethodID writeMethod_;

    std::function<void(const uint8_t*, size_t)> dataCallback_;
    std::function<void(bool)> connectionCallback_;
    std::atomic<bool> connected_{false};
    std::mutex mutex_;
	std::string deviceAddress_;

    JNIEnv* getJNIEnv();        // uses Attach/Detach
    void cleanup();
};

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_BleManager_nativeOnDataReceived(JNIEnv* env, jobject /*thiz*/, jlong transportPtr, jbyteArray data);

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_BleManager_nativeOnConnectionStateChanged(JNIEnv* env, jobject /*thiz*/, jlong transportPtr, jboolean connected);