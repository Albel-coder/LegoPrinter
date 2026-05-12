#include "printer_jni.h"
#include "converter.h"
#include "../include/TransportAndroid.h"
#include "../include/LegoPrinterCore.h"
#include <android/log.h>

#define LOG_TAG "PrinterJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using Printer = PrinterDriver;

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_printer_PrinterController_createPrinter(JNIEnv *env, jobject /*thiz*/,
                                                          jobject context) {
    // Create transport for android
    JavaVM *jvm = nullptr;
    env->GetJavaVM(&jvm);
    jobject globalContext = env->NewGlobalRef(context);
    std::unique_ptr<ITransport> transport = std::make_unique<TransportAndroid>(jvm, globalContext);

    // Create printer
    auto *printer = new PrinterDriver(std::move(transport));
    LOGI("Printer created: %p", printer);
    return reinterpret_cast<jlong>(printer);
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_destroyPrinter(JNIEnv *env, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer*>(handle);
    if (printer) {
        delete printer;
        LOGI("Printer destroyed: %p", printer);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerConnect(JNIEnv *, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return JNI_FALSE;
    return printer->connect() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerDisconnect(JNIEnv *, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return JNI_FALSE;
    return printer->disconnect() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_isConnected(JNIEnv *, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return JNI_FALSE;
    return printer->isConnected() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerRotateMotor(JNIEnv *env, jobject, jlong handle,
                                                               jobjectArray commandsArray,
                                                               jint count) {

    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer || !commandsArray || count < 1) return;

    auto commands = convertMotorCommands(env, commandsArray);
    if (!commands.empty()) {
        printer->rotateMotor(commands.data(), static_cast<int>(commands.size()));
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerSendCommand(JNIEnv *env, jobject, jlong handle,
                                                               jbyteArray commandArray,
                                                               jint length) {

    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer || !commandArray || length < 1) return;

    jbyte *bytes = env->GetByteArrayElements(commandArray, nullptr);
    if (bytes) {
        printer->sendCommand(reinterpret_cast<const uint8_t *>(bytes), static_cast<size_t>(length));
        env->ReleaseByteArrayElements(commandArray, bytes, 0);
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerSetMotorSpeed(JNIEnv *, jobject, jlong handle,
                                                                 jbyte port, jbyte speed) {

    auto *printer = reinterpret_cast<Printer *>(handle);
    if (printer) {
        printer->setMotorSpeed(static_cast<uint8_t>(port), static_cast<int8_t>(speed));
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_printer_PrinterController_getLogCount(JNIEnv *, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return 0;
    return printer->getLogCount();
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_printer_PrinterController_getLogEntry(JNIEnv *env, jobject, jlong handle,
                                                        jint index) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return env->NewStringUTF("");
    const char *entry = printer->getLogEntry(index);
    return entry ? env->NewStringUTF(entry) : env->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_clearLog(JNIEnv *, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (printer) printer->clearLog();
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_printer_PrinterController_getLastErrorMessage(JNIEnv *env, jobject,
                                                                jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return env->NewStringUTF("");
    const char *err = printer->getLastErrorMessage();
    return err ? env->NewStringUTF(err) : env->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerConnectionInfo(JNIEnv *env, jobject,
                                                                  jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (printer) {
        printer->printerConnectionInfo();
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerSetLogCategories(JNIEnv *, jobject, jlong handle,
                                                                    jint categories) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (printer) {
        printer->printerSetLogCategories(categories);
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerGetLogCategories(JNIEnv *, jobject,
                                                                    jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return 0;
    return printer->printerGetLogCategories();
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerExecuteSpeedProfile(JNIEnv *env, jobject,
                                                                       jlong handle,
                                                                       jobject jProfile) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer || !jProfile) return JNI_FALSE;
    auto profile = convertSpeedProfile(env, jProfile);
    if (profile.points == nullptr) return JNI_FALSE;
    return printer->printerExecuteSpeedProfile(&profile) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerExecuteSpeedProfiles(JNIEnv *env, jobject,
                                                                        jlong handle,
                                                                        jobjectArray profilesArray,
                                                                        jint count) {

    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer || !profilesArray || count < 1) return JNI_FALSE;
    auto profiles = convertSpeedProfiles(env, profilesArray);
    if (profiles.empty()) return JNI_FALSE;
    return printer->printerExecuteSpeedProfiles(profiles.data(), profiles.size()) ? JNI_TRUE
                                                                                  : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerIsMotorMoving(JNIEnv *, jobject, jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return JNI_FALSE;
    return printer->printerIsMotorMoving(0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerGetMotorPosition(JNIEnv *, jobject, jlong handle,
                                                                    jbyte port) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return 0.0;
    return printer->printerGetMotorPosition(static_cast<uint8_t>(port)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_example_lpstudio_printer_PrinterController_runPrinterTest(
        JNIEnv *env, jobject thiz, jlong handle, jstring testName) {

    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer || !testName) return JNI_FALSE;

    const char *nativeString = env->GetStringUTFChars(testName, nullptr);
    if (!nativeString) return JNI_FALSE;

    bool result = printer->runPrinterTest(nativeString);
    env->ReleaseStringUTFChars(testName, nativeString);

    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerRequestBatteryLevel(JNIEnv *, jobject,
                                                                       jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return JNI_FALSE;
    return printer->printerRequestBatteryLevel() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jbyte JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerGetBatteryLevel(JNIEnv *, jobject,
                                                                   jlong handle) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer) return 0;
    return printer->printerGetBatteryLevel();
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_PrinterController_printerIsBatteryLevelFresh(JNIEnv *, jobject,
                                                                       jlong handle,
                                                                       jint maxAgeSeconds) {
    auto *printer = reinterpret_cast<Printer *>(handle);
    if (!printer || maxAgeSeconds < 1) return JNI_FALSE;
    return printer->printerIsBatteryLevelFresh(maxAgeSeconds) ? JNI_TRUE : JNI_FALSE;
}

#ifdef __cplusplus
}
#endif