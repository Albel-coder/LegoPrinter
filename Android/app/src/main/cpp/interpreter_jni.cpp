#include "interpreter_jni.h"
#include "../include/InterpreterAPI.h"
#include "../include/LegoDriverAPI.h"
#include <android/log.h>

#define LOG_TAG "InterpreterJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using Printer = PrinterDriver;

JNIEXPORT jlong JNICALL
Java_com_example_GCodeInterpreter_createInterpreter(JNIEnv* env, jobject) {
	auto* interpreter = new Interpreter();
	LOGI("Interpreter created: %p", interpreter);
	return reinterpret_cast<jlong>(interpreter);
}

Java_com_example_GCodeInterpreter_destroyInterpreter(JNIEnv* env, jobject, jlong handle) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (interpreter) {
		delete interpreter;
		LOGI("Interpreter destroyed: %p", interpreter);
	}
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeGCode(JNIEnv* env, jobject, 
    jlong interpreterHandle, jstring filename, jlong printerHandle) {
	
	auto* interpreter = reinterpret_cast<Interpreter*>(interpreterHandle);
	auto* printer = reinterpret_cast<Printer*>(printerHandle);
	if (!interpreter || !printer || !filename) return JNI_FALSE;
	
	const char* c_filename = env->GetStringUTFChars(filename, nullptr);
	if (!c_filename) return JNI_FALSE;
	
	bool result = interpreter->executeFile(c_filename, printer);
	env->ReleaseStringUTFChars(filename, c_filename);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeLine(JNIEnv* env, jobject, 
    jlong interpreterHandle, jstring line, jlong printerHandle) {
	
	auto* interpreter = reinterpret_cast<Interpreter*>(interpreterHandle);
	auto* printer = reinterpret_cast<Printer*>(printerHandle);
	if (!interpreter || !printer || !line) return JNI_FALSE;
	
	const char* c_line = env->GetStringUTFChars(line, nullptr);
	if (!c_line) return JNI_FALSE;
	
	bool result = interpreter->executeLine(c_line, printer);
	env->ReleaseStringUTFChars(line, c_line);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_pauseExecution(JNIEnv* env, jobject, jlong interpreterHandle) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (interpreter) interpreter->pause();
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_resumeExecution(JNIEnv* env, jobject, jlong interpreterHandle) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (interpreter) interpreter->resumeExecution();
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getStatus(JNIEnv* env, jobject, jlong interpreterHandle) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (!interpreter) return 0;
	return static_cast<jint>(interpreter->getStatus());
}

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getProgress(JNIEnv*, jobject, jlong interpreterHandle) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (!interpreter) return 0.0;
	return interpreter->getProgress();
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLastInterpreterError(JNIEnv* env, jobject, jlong interpreterHandle) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (!interpreter) return env->NewStringUTF("");
	const char* err = interpreter->getLastInterpreterError();
	return err ? env->NewStringUTF(err) : env->NewStringUTF("");
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_readConfig(JNIEnv* env, jobject, jlong interpreterHandle, jstring filename) {
	auto* interpreter = reinterpret_cast<Interpreter*>(handle);
	if (!interpreter || !filename) return JNI_FALSE;
	
	const char* c_filename = env->GetStringUTFChars(filename, nullptr);
	if (c_filename) return JNI_FALSE;
	
	bool result = interpreter->readConfig(c_filename);
	env->ReleaseStringUTFChars(filename, c_filename);
	return result ? JNI_TRUE : JNI_FALSE;
}