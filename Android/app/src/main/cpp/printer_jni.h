#pragma once

#include <jni.h>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_PrinterController_createPrinter(JNIEnv* env, jobject /*thiz*/, jobject context);

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_destroyPrinter(JNIEnv* env, jobject, jlong handle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerConnect(JNIEnv*, jobject, jlong handle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerDisconnect(JNIEnv*, jobject, jlong handle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_isConnected(JNIEnv*, jobject, jlong handle);

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerRotateMotor(JNIEnv* env, jobject, jlong handle, 
    jobjectArray commandsArray, jint count);
	
JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSendCommand(JNIEnv* env, jobject, jlong handle, 
    jbyteArray commandArray, jint length);
	
JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetMotorSpeed(JNIEnv*, jobject, jlong handle,
	jbyte port, jbyte speed);
	
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_getLogCount(JNIEnv*, jobject, jlong handle);

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLogEntry(JNIEnv* env, jobject, jlong handle, jint index);

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_clearLog(JNIEnv*, jobject, jlong handle);

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLastErrorMessage(JNIEnv* env, jobject, jlong handle);

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_printerConnectionInfo(JNIEnv* env, jobject, jlong handle);

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetLogCategories(JNIEnv*, jobject, jlong handle, jint categories);

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_printerGetLogCategories(JNIEnv*, jobject, jlong handle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfile(JNIEnv* env, jobject, jlong handle,
 jobject jProfile);
 
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfiles(JNIEnv* env, jobject, jlong handle, 
    jobjectArray profilesArray, jint count)
	
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsMotorMoving(JNIEnv*, jobject, jlong handle);

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_PrinterController_printerGetMotorPosition(JNIEnv*, jobject, jlong handle, jbyte port);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_runPrinterTest(JNIEnv* env, jobject, jlong handle, jstring testName);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerRequestBatteryLevel(JNIEnv*, jobject, jlong handle);

JNIEXPORT jbyte JNICALL
Java_com_example_lpstudio_PrinterController_printerGetBatteryLevel(JNIEnv*, jobject, jlong handle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsBatteryLevelFresh(JNIEnv*, jobject, jlong handle, jint maxAgeSeconds);

#ifdef __cplusplus
}
#endif