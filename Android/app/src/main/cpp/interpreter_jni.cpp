#include "printer_jni.h"

extern "C" {

// ========== Lifecycle ==========
JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_GCodeInterpreter_createInterpreter(JNIEnv*, jobject) {
    return reinterpret_cast<jlong>(createInterpreterRuntime());
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_destroyInterpreter(JNIEnv*, jobject, jlong runtimePtr) {
    destroyInterpreterRuntime(reinterpret_cast<InterpreterRuntime*>(runtimePtr));
}

// ========== Execution ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_testCode(
    JNIEnv*, jobject, jlong interpreterRuntimePtr, jlong printerRuntimePtr) {
    
    bool result = false;
    WITH_INTERPRETER_AND_PRINTER(interpreterRuntimePtr, printerRuntimePtr, {
        result = TestCode(interpreter, printer);
    });
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeGCode(
    JNIEnv* env, jobject, 
    jlong interpreterRuntimePtr, jstring filename, 
    jlong printerRuntimePtr) {
    
    if (!filename) return JNI_FALSE;
    
    const char* c_filename = env->GetStringUTFChars(filename, nullptr);
    if (!c_filename) return JNI_FALSE;
    
    bool result = false;
    WITH_INTERPRETER_AND_PRINTER(interpreterRuntimePtr, printerRuntimePtr, {
        result = ExecuteGcode(interpreter, c_filename, printer);
    });
    
    env->ReleaseStringUTFChars(filename, c_filename);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeLine(
    JNIEnv* env, jobject, 
    jlong interpreterRuntimePtr, jstring line, 
    jlong printerRuntimePtr) {
    
    if (!line) return JNI_FALSE;
    
    const char* c_line = env->GetStringUTFChars(line, nullptr);
    if (!c_line) return JNI_FALSE;
    
    bool result = false;
    WITH_INTERPRETER_AND_PRINTER(interpreterRuntimePtr, printerRuntimePtr, {
        result = ExecuteLine(interpreter, c_line, printer);
    });
    
    env->ReleaseStringUTFChars(line, c_line);
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Control ==========
JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_pauseExecution(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    WITH_INTERPRETER(interpreterRuntimePtr, {
        PauseExecution(interpreter);
    });
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_resumeExecution(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    WITH_INTERPRETER(interpreterRuntimePtr, {
        ResumeExecution(interpreter);
    });
}

// ========== Status and Info ==========
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getStatus(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    jint result = 0;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        result = GetStatus(interpreter);
    });
    return result;
}

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getProgress(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    jdouble result = 0.0;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        result = GetProgress(interpreter);
    });
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLastInterpreterError(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr) {
    
    const char* error = nullptr;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        error = GetLastInterpreterError(interpreter);
    });
    return error ? env->NewStringUTF(error) : env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getError(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr, jint index) {
    
    const char* error = nullptr;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        error = GetError(interpreter, index);
    });
    return error ? env->NewStringUTF(error) : env->NewStringUTF("");
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getErrorCount(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    // Простая реализация - считаем ошибки до первой пустой
    jint count = 0;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        for (int i = 0; i < 1000; i++) { // Практический лимит
            const char* error = GetError(interpreter, i);
            if (!error || error[0] == '\0') break;
            count++;
        }
    });
    return count;
}

// ========== Logging ==========
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterLogCount(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    jint result = 0;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        result = GetInterpreterLogCount(interpreter);
    });
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterLogEntry(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr, jint index) {
    
    const char* log = nullptr;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        log = GetInterpreterLogEntry(interpreter, index);
    });
    return log ? env->NewStringUTF(log) : env->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_clearInterpreterLog(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    WITH_INTERPRETER(interpreterRuntimePtr, {
        ClearInterpreterLog(interpreter);
    });
}

// ========== Configuration ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_readConfig(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr, jstring filename) {
    
    if (!filename) return JNI_FALSE;
    
    const char* c_filename = env->GetStringUTFChars(filename, nullptr);
    if (!c_filename) return JNI_FALSE;
    
    bool result = false;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        result = ReadConfig(interpreter, c_filename);
    });
    
    env->ReleaseStringUTFChars(filename, c_filename);
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Log Categories ==========
JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_setLogCategories(
    JNIEnv*, jobject, jlong interpreterRuntimePtr, jint categories) {
    
    WITH_INTERPRETER(interpreterRuntimePtr, {
        SetLogCategories(interpreter, static_cast<unsigned int>(categories));
    });
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLogCategories(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    jint result = 0;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        result = static_cast<jint>(GetLogCategories(interpreter));
    });
    return result;
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getFilterLogCount(
    JNIEnv*, jobject, jlong interpreterRuntimePtr, jint categoryMask) {
    
    jint result = 0;
    WITH_INTERPRETER(interpreterRuntimePtr, {
        result = GetFilterLogCount(interpreter, static_cast<unsigned int>(categoryMask));
    });
    return result;
}

// ========== Utility Methods ==========
JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterVersion(JNIEnv* env, jobject) {
    const char* version = "GCode Interpreter JNI v2.1 (Optimized)";
    return env->NewStringUTF(version);
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_checkInterpreterValid(JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    if (!isJNIInitialized() || !interpreterRuntimePtr) return JNI_FALSE;
    
    auto* runtime = reinterpret_cast<InterpreterRuntime*>(interpreterRuntimePtr);
    std::lock_guard<std::mutex> lock(runtime->mutex);
    return (runtime->alive && runtime->interpreter) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
