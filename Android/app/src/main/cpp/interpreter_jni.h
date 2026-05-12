#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_createInterpreter(JNIEnv* env, jobject, jlong printerHandle);

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_destroyInterpreter(JNIEnv* env, jobject, jlong handle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_executeGCode(JNIEnv* env, jobject, 
    jlong interpreterHandle, jstring filename, jlong printerHandle);
	
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_executeLine(JNIEnv* env, jobject, 
    jlong interpreterHandle, jstring line, jlong printerHandle);
	
JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_pauseExecution(JNIEnv* env, jobject, jlong interpreterHandle);

JNIEXPORT void JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_resumeExecution(JNIEnv* env, jobject, jlong interpreterHandle);

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_getStatus(JNIEnv* env, jobject, jlong interpreterHandle);

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_getProgress(JNIEnv*, jobject, jlong interpreterHandle);

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_getLastInterpreterError(JNIEnv* env, jobject, jlong interpreterHandle);

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_printer_GCodeInterpreter_readConfig(JNIEnv* env, jobject, jlong interpreterHandle, jstring filename);

#ifdef __cplusplus
}
#endif