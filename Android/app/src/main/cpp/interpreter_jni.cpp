#include "printer_jni.h"
#include "jni_safe_call.h"

extern "C" {

// ========== Lifecycle Management ==========
JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_GCodeInterpreter_createInterpreter(JNIEnv*, jobject) {
    JNISafeCall guard;
    if (!guard.valid()) {
        JNI_LOGE("JNI not initialized in createInterpreter");
        return 0;
    }
    
    try {
        InterpreterRuntime* runtime = createInterpreterRuntime();
        if (runtime) {
            JNI_LOGI("InterpreterRuntime created successfully: %p", runtime);
            return reinterpret_cast<jlong>(runtime);
        } else {
            JNI_LOGE("Failed to create InterpreterRuntime");
            return 0;
        }
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in createInterpreter: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in createInterpreter");
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_destroyInterpreter(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (runtimePtr == 0) {
        JNI_LOGE("Invalid runtime handle in destroyInterpreter");
        return;
    }
    
    try {
        destroyInterpreterRuntime(reinterpret_cast<InterpreterRuntime*>(runtimePtr));
        JNI_LOGI("InterpreterRuntime destroyed successfully");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in destroyInterpreter: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in destroyInterpreter");
    }
}

// ========== Execution Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_testCode(
    JNIEnv*, jobject, jlong interpreterRuntimePtr, jlong printerRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0 || printerRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in testCode");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        JNI_LOGI("Testing G-code interpreter with printer");
        WITH_INTERPRETER_AND_PRINTER(interpreterRuntimePtr, printerRuntimePtr, {
            result = TestCode(interpreter, printer);
        });
        JNI_LOGI("G-code test result: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in testCode: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in testCode");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeGCode(
    JNIEnv* env, jobject, 
    jlong interpreterRuntimePtr, jstring filename, 
    jlong printerRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0 || printerRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in executeGCode");
        return JNI_FALSE;
    }
    
    if (!filename) {
        JNI_LOGE("Filename is null in executeGCode");
        return JNI_FALSE;
    }
    
    const char* c_filename = guard.getStringUTFChars(filename, nullptr);
    if (!c_filename) {
        JNI_LOGE("Failed to get filename string in executeGCode");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        JNI_LOGI("Executing G-code file: %s", c_filename);
        WITH_INTERPRETER_AND_PRINTER(interpreterRuntimePtr, printerRuntimePtr, {
            result = ExecuteGcode(interpreter, c_filename, printer);
        });
        JNI_LOGI("G-code execution result: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in executeGCode: %s", e.what());
        guard.releaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in executeGCode");
        guard.releaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    }
    
    guard.releaseStringUTFChars(filename, c_filename);
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeLine(
    JNIEnv* env, jobject, 
    jlong interpreterRuntimePtr, jstring line, 
    jlong printerRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0 || printerRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in executeLine");
        return JNI_FALSE;
    }
    
    if (!line) {
        JNI_LOGE("Line is null in executeLine");
        return JNI_FALSE;
    }
    
    const char* c_line = guard.getStringUTFChars(line, nullptr);
    if (!c_line) {
        JNI_LOGE("Failed to get line string in executeLine");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        JNI_LOGI("Executing G-code line: %s", c_line);
        WITH_INTERPRETER_AND_PRINTER(interpreterRuntimePtr, printerRuntimePtr, {
            result = ExecuteLine(interpreter, c_line, printer);
        });
        JNI_LOGI("G-code line execution result: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in executeLine: %s", e.what());
        guard.releaseStringUTFChars(line, c_line);
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in executeLine");
        guard.releaseStringUTFChars(line, c_line);
        return JNI_FALSE;
    }
    
    guard.releaseStringUTFChars(line, c_line);
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Control Methods ==========
JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_pauseExecution(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in pauseExecution");
        return;
    }
    
    try {
        JNI_LOGI("Pausing G-code execution");
        WITH_INTERPRETER(interpreterRuntimePtr, {
            PauseExecution(interpreter);
        });
        JNI_LOGI("G-code execution paused");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in pauseExecution: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in pauseExecution");
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_resumeExecution(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in resumeExecution");
        return;
    }
    
    try {
        JNI_LOGI("Resuming G-code execution");
        WITH_INTERPRETER(interpreterRuntimePtr, {
            ResumeExecution(interpreter);
        });
        JNI_LOGI("G-code execution resumed");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in resumeExecution: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in resumeExecution");
    }
}

// ========== Status and Info Methods ==========
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getStatus(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getStatus");
        return 0;
    }
    
    jint result = 0;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            result = GetStatus(interpreter);
        });
        JNI_LOGD("Interpreter status: %d", result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getStatus: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getStatus");
        return 0;
    }
    
    return result;
}

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getProgress(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getProgress");
        return 0.0;
    }
    
    jdouble result = 0.0;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            result = GetProgress(interpreter);
        });
        JNI_LOGD("Interpreter progress: %.2f%%", result * 100);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getProgress: %s", e.what());
        return 0.0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getProgress");
        return 0.0;
    }
    
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLastInterpreterError(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getLastInterpreterError");
        return guard.valid() ? guard.env()->NewStringUTF("") : nullptr;
    }
    
    const char* error = nullptr;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            error = GetLastInterpreterError(interpreter);
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getLastInterpreterError: %s", e.what());
        return guard.env()->NewStringUTF("");
    } catch (...) {
        JNI_LOGE("Unknown exception in getLastInterpreterError");
        return guard.env()->NewStringUTF("");
    }
    
    return error ? guard.env()->NewStringUTF(error) : guard.env()->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getError(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr, jint index) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getError");
        return guard.valid() ? guard.env()->NewStringUTF("") : nullptr;
    }
    
    const char* error = nullptr;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            error = GetError(interpreter, index);
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getError: %s", e.what());
        return guard.env()->NewStringUTF("");
    } catch (...) {
        JNI_LOGE("Unknown exception in getError");
        return guard.env()->NewStringUTF("");
    }
    
    return error ? guard.env()->NewStringUTF(error) : guard.env()->NewStringUTF("");
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getErrorCount(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getErrorCount");
        return 0;
    }
    
    jint count = 0;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            for (int i = 0; i < 1000; i++) {
                const char* error = GetError(interpreter, i);
                if (!error || error[0] == '\0') break;
                count++;
            }
        });
        JNI_LOGD("Error count: %d", count);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getErrorCount: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getErrorCount");
        return 0;
    }
    
    return count;
}

// ========== Logging Methods ==========
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterLogCount(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getInterpreterLogCount");
        return 0;
    }
    
    jint result = 0;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            result = GetInterpreterLogCount(interpreter);
        });
        JNI_LOGD("Interpreter log count: %d", result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getInterpreterLogCount: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getInterpreterLogCount");
        return 0;
    }
    
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterLogEntry(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr, jint index) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getInterpreterLogEntry");
        return guard.valid() ? guard.env()->NewStringUTF("") : nullptr;
    }
    
    const char* log = nullptr;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            log = GetInterpreterLogEntry(interpreter, index);
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getInterpreterLogEntry: %s", e.what());
        return guard.env()->NewStringUTF("");
    } catch (...) {
        JNI_LOGE("Unknown exception in getInterpreterLogEntry");
        return guard.env()->NewStringUTF("");
    }
    
    return log ? guard.env()->NewStringUTF(log) : guard.env()->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_clearInterpreterLog(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in clearInterpreterLog");
        return;
    }
    
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            ClearInterpreterLog(interpreter);
        });
        JNI_LOGI("Interpreter log cleared");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in clearInterpreterLog: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in clearInterpreterLog");
    }
}

// ========== Configuration Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_readConfig(
    JNIEnv* env, jobject, jlong interpreterRuntimePtr, jstring filename) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in readConfig");
        return JNI_FALSE;
    }
    
    if (!filename) {
        JNI_LOGE("Filename is null in readConfig");
        return JNI_FALSE;
    }
    
    const char* c_filename = guard.getStringUTFChars(filename, nullptr);
    if (!c_filename) {
        JNI_LOGE("Failed to get filename string in readConfig");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        JNI_LOGI("Reading config file: %s", c_filename);
        WITH_INTERPRETER(interpreterRuntimePtr, {
            result = ReadConfig(interpreter, c_filename);
        });
        JNI_LOGI("Config read result: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in readConfig: %s", e.what());
        guard.releaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in readConfig");
        guard.releaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    }
    
    guard.releaseStringUTFChars(filename, c_filename);
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Log Categories Methods ==========
JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_setLogCategories(
    JNIEnv*, jobject, jlong interpreterRuntimePtr, jint categories) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in setLogCategories");
        return;
    }
    
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            SetLogCategories(interpreter, static_cast<unsigned int>(categories));
        });
        JNI_LOGI("Interpreter log categories set to: 0x%X", categories);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in setLogCategories: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in setLogCategories");
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLogCategories(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getLogCategories");
        return 0;
    }
    
    jint result = 0;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            result = static_cast<jint>(GetLogCategories(interpreter));
        });
        JNI_LOGD("Current interpreter log categories: 0x%X", result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getLogCategories: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getLogCategories");
        return 0;
    }
    
    return result;
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getFilterLogCount(
    JNIEnv*, jobject, jlong interpreterRuntimePtr, jint categoryMask) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in getFilterLogCount");
        return 0;
    }
    
    jint result = 0;
    try {
        WITH_INTERPRETER(interpreterRuntimePtr, {
            result = GetFilterLogCount(interpreter, static_cast<unsigned int>(categoryMask));
        });
        JNI_LOGD("Filtered log count (mask 0x%X): %d", categoryMask, result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getFilterLogCount: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getFilterLogCount");
        return 0;
    }
    
    return result;
}

// ========== Utility Methods ==========
JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterVersion(JNIEnv* env, jobject) {
    JNISafeCall guard;
    if (!guard.valid()) {
        JNI_LOGE("JNI not initialized in getInterpreterVersion");
        return nullptr;
    }
    
    const char* version = "GCode Interpreter JNI v2.1 (SafeCall)";
    return guard.env()->NewStringUTF(version);
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_checkInterpreterValid(
    JNIEnv*, jobject, jlong interpreterRuntimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || interpreterRuntimePtr == 0) {
        JNI_LOGE("Invalid parameters in checkInterpreterValid");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        auto* runtime = reinterpret_cast<InterpreterRuntime*>(interpreterRuntimePtr);
        std::lock_guard<std::mutex> lock(runtime->mutex);
        result = (runtime->alive && runtime->interpreter);
        JNI_LOGD("Interpreter valid check: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in checkInterpreterValid: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in checkInterpreterValid");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

} // extern "C”
