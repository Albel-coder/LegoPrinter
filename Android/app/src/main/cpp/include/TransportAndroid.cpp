#include "TransportAndroid.h"
#include <android/log.h>
#include <cassert>

#define LOG_TAG "TransportAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper RAII class for JNIEnv (similar to JNISafeCall, but local)
class ScopedJNIEnv {
public:
    ScopedJNIEnv(JavaVM* jvm) : jvm_(jvm), env_(nullptr), attached_(false) {
        if (!jvm_) return;
        jint result = jvm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED) {
            result = jvm_->AttachCurrentThread(&env_, nullptr);
            attached_ = (result == JNI_OK);
        }
    }
    ~ScopedJNIEnv() {
        if (attached_ && jvm_) {
            jvm_->DetachCurrentThread();
        }
    }
    JNIEnv* operator->() const { return env_; }
    operator bool() const { return env_ != nullptr; }
private:
    JavaVM* jvm_;
    JNIEnv* env_;
    bool attached_;
};

TransportAndroid::TransportAndroid(JavaVM* jvm, jobject context)
    : jvm_(jvm),
    bleManager_(nullptr),
    openMethod_(nullptr),
    closeMethod_(nullptr),
    writeMethod_(nullptr),
    connected_(false) {
    ScopedJNIEnv env(jvm_);
    if (!env) {
        LOGE("Failed to get JNIEnv");
        return;
    }

    // Create a BleManager instance in Kotlin
	jclass contextClass = env->GetObjectClass(context);
    jclass bleManagerClass = env->FindClass("com/example/lpstudio/BleManager");
    if (!bleManagerClass) {
        LOGE("BleManager class not found");
        return;
    }

    jmethodID constructor = env->GetMethodID(bleManagerClass, "<init>", "(Landroid/content/Context;)V");
	if (!constructor) {
		LOGE("BleManager constructor not found");
		return;
	}
    jobject localBleManager = env->NewObject(bleManagerClass, constructor, context);
    bleManager_ = env->NewGlobalRef(localBleManager);
    env->DeleteLocalRef(localBleManager);
    env->DeleteLocalRef(bleManagerClass);

    // Get methods
    openMethod_ = env->GetMethodID(env->GetObjectClass(bleManager_), "open", "()Z");
	if (!openMethod_) {
		LOGE("BleManager openMethod_ not found");
		return;
	}
    closeMethod_ = env->GetMethodID(env->GetObjectClass(bleManager_), "close", "()V");
	if (!closeMethod_) {
		LOGE("BleManager closeMethod_ not found");
		return;
	}
    writeMethod_ = env->GetMethodID(env->GetObjectClass(bleManager_), "write", "([B)Z");
	if (!writeMethod_) {
		LOGE("BleManager writeMethod_ not found");
		return;
	}
}

TransportAndroid::~TransportAndroid() {
    cleanup();
}

void TransportAndroid::cleanup() {
    ScopedJNIEnv env(jvm_);
    if (env && bleManager_) {
        env->DeleteGlobalRef(bleManager_);
        bleManager_ = nullptr;
    }
}

bool TransportAndroid::open() {
	ScopedJNIEnv env(jvm_);
    if (!env || !bleManager_ || !openMethod_) return false;
    jboolean result = env->CallBooleanMethod(bleManager_, openMethod_);
    return result == JNI_TRUE;
}

void TransportAndroid::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedJNIEnv env(jvm_);
    if (env && bleManager_) {
        env->CallVoidMethod(bleManager_, closeMethod_);
    }
    connected_ = false;
}

bool TransportAndroid::write(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return false;

    ScopedJNIEnv env(jvm_);
    if (!env || !bleManager_) return false;

    jbyteArray byteArray = env->NewByteArray(static_cast<jsize>(length));
    env->SetByteArrayRegion(byteArray, 0, static_cast<jsize>(length),
                            reinterpret_cast<const jbyte*>(data));

    jboolean result = env->CallBooleanMethod(bleManager_, writeMethod_, byteArray);
    env->DeleteLocalRef(byteArray);
    return result;
}

bool TransportAndroid::isConnected() {
    return connected_.load();
}

void TransportAndroid::setDataCallback(std::function<void(const uint8_t*, size_t)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    dataCallback_ = std::move(callback);
}

void TransportAndroid::setConnectionCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    connectionCallback_ = std::move(callback);
}

void TransportAndroid::onDataReceived(JNIEnv* env, jbyteArray data) {
    jsize len = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    std::vector<uint8_t> buffer(bytes, bytes + len);

    std::function<void(const uint8_t*, size_t)> cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = dataCallback_;
    }
    if (cb) {
        cb(buffer.data(), buffer.size());
    }
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
}

void TransportAndroid::onConnectionStateChanged(JNIEnv* env, jboolean connected) {
    connected_ = connected;
    std::function<void(bool)> cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = connectionCallback_;
    }
    if (cb) {
        cb(connected);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_BleManager_nativeOnDataReceived(JNIEnv* env, jobject /*thiz*/, jlong transportPtr, jbyteArray data) {
    auto* transport = reinterpret_cast<TransportAndroid*>(transportPtr);
    if (!transport) return;
    jsize len = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    if (bytes) {
        transport->onDataReceived(env, data);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_BleManager_nativeOnConnectionStateChanged(JNIEnv* env, jobject /*thiz*/, jlong transportPtr, jboolean connected) {
    auto* transport = reinterpret_cast<TransportAndroid*>(transportPtr);
    if (!transport) return;
    transport->onConnectionStateChanged(env, connected);
}

