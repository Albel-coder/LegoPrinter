#include <jni.h>
#include <string>
#include <vector>
#include "jni_util.h"

// Включаем заголовок интерпретатора
#include "interpreter.h"

// Вспомогательные функции преобразования
InterpreterHandle toInterpreterHandle(jlong handle) {
    return reinterpret_cast<InterpreterHandle>(handle);
}

IPrinter* toPrinterHandle(jlong handle) {
    return reinterpret_cast<IPrinter*>(handle);
}

jstring toJavaString(JNIEnv* env, const char* cstr) {
    if (!cstr) {
        return env->NewStringUTF("");
    }
    return env->NewStringUTF(cstr);
}

// Проверка валидности дескрипторов
bool CheckInterpreterHandles(jlong interpreterPtr, jlong printerPtr) {
    if (!IsJNIInitialized()) {  // Теперь используем bool
        LOGE("JNI not initialized!");
        return false;
    }
    
    if (interpreterPtr == 0) {
        LOGE("Interpreter handle is null!");
        return false;
    }
    
    if (printerPtr == 0) {
        LOGE("Printer handle is null!");
        return false;
    }
    
    return true;
}


// =========== JNI методы интерпретатора ==========

#ifdef __cplusplus
extern "C" {
#endif

// 1. Управление жизненным циклом
JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_GCodeInterpreter_createInterpreter(JNIEnv* env, jobject thiz) {
    LOGI("createInterpreter called");
    
    if (!IsJNIInitialized()) {
        LOGE("JNI not initialized!");
        return 0;
    }
    
    try {
        InterpreterHandle handle = CreateInterpreter();
        LOGI("Interpreter created: %p", handle);
        return reinterpret_cast<jlong>(handle);
    } catch (const std::exception& e) {
        LOGE("Failed to create interpreter: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("Failed to create interpreter: unknown exception");
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_destroyInterpreter(JNIEnv* env, jobject thiz, jlong interpreterPtr) {
    if (interpreterPtr == 0) return;
    
    LOGI("destroyInterpreter called for: %p", (void*)interpreterPtr);
    
    try {
        InterpreterHandle handle = toInterpreterHandle(interpreterPtr);
        DestroyInterpreter(handle);
        LOGI("Interpreter destroyed");
    } catch (const std::exception& e) {
        LOGE("Failed to destroy interpreter: %s", e.what());
    } catch (...) {
        LOGE("Failed to destroy interpreter: unknown exception");
    }
}

// 2. Методы выполнения
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_testCode(JNIEnv* env, jobject thiz, 
                                                   jlong interpreterPtr, jlong printerHandle) {
    
    if (!CheckInterpreterHandles(interpreterPtr, printerHandle)) {
        return JNI_FALSE;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        IPrinter* printer = toPrinterHandle(printerHandle);
        
        LOGI("Testing code with interpreter: %p, printer: %p", interpreter, printer);
        bool result = TestCode(interpreter, printer);
        
        LOGI("TestCode result: %s", result ? "true" : "false");
        return result ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("TestCode failed: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("TestCode failed: unknown exception");
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeGCode(JNIEnv* env, jobject thiz, 
                                                       jlong interpreterPtr, jstring filename, 
                                                       jlong printerHandle) {
    
    if (!CheckInterpreterHandles(interpreterPtr, printerHandle)) {
        return JNI_FALSE;
    }
    
    if (!filename) {
        LOGE("Filename is null!");
        return JNI_FALSE;
    }
    
    const char* c_filename = env->GetStringUTFChars(filename, nullptr);
    if (!c_filename) {
        LOGE("Failed to get filename string");
        return JNI_FALSE;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        IPrinter* printer = toPrinterHandle(printerHandle);
        
        LOGI("Executing G-code file: %s", c_filename);
        bool result = ExecuteGcode(interpreter, c_filename, printer);
        LOGI("ExecuteGcode result: %s", result ? "true" : "false");
        
        env->ReleaseStringUTFChars(filename, c_filename);
        return result ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("ExecuteGcode failed: %s", e.what());
        env->ReleaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    } catch (...) {
        LOGE("ExecuteGcode failed: unknown exception");
        env->ReleaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_executeLine(JNIEnv* env, jobject thiz, 
                                                      jlong interpreterPtr, jstring line, 
                                                      jlong printerHandle) {
    
    if (!CheckInterpreterHandles(interpreterPtr, printerHandle)) {
        return JNI_FALSE;
    }
    
    if (!line) {
        LOGE("Line is null!");
        return JNI_FALSE;
    }
    
    const char* c_line = env->GetStringUTFChars(line, nullptr);
    if (!c_line) {
        LOGE("Failed to get line string");
        return JNI_FALSE;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        IPrinter* printer = toPrinterHandle(printerHandle);
        
        LOGI("Executing line: %s", c_line);
        bool result = ExecuteLine(interpreter, c_line, printer);
        LOGI("ExecuteLine result: %s", result ? "true" : "false");
        
        env->ReleaseStringUTFChars(line, c_line);
        return result ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("ExecuteLine failed: %s", e.what());
        env->ReleaseStringUTFChars(line, c_line);
        return JNI_FALSE;
    } catch (...) {
        LOGE("ExecuteLine failed: unknown exception");
        env->ReleaseStringUTFChars(line, c_line);
        return JNI_FALSE;
    }
}

// 3. Методы управления
JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_pauseExecution(JNIEnv* env, jobject thiz, 
                                                         jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        LOGE("Interpreter handle is null in pauseExecution");
        return;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        PauseExecution(interpreter);
        LOGI("Execution paused");
    } catch (const std::exception& e) {
        LOGE("PauseExecution failed: %s", e.what());
    } catch (...) {
        LOGE("PauseExecution failed: unknown exception");
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_resumeExecution(JNIEnv* env, jobject thiz, 
                                                          jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        LOGE("Interpreter handle is null in resumeExecution");
        return;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        ResumeExecution(interpreter);
        LOGI("Execution resumed");
    } catch (const std::exception& e) {
        LOGE("ResumeExecution failed: %s", e.what());
    } catch (...) {
        LOGE("ResumeExecution failed: unknown exception");
    }
}

// 4. Методы статуса и информации
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getStatus(JNIEnv* env, jobject thiz, 
                                                    jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        LOGE("Interpreter handle is null in getStatus");
        return 0; // IDLE
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        return GetStatus(interpreter);
    } catch (const std::exception& e) {
        LOGE("GetStatus failed: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("GetStatus failed: unknown exception");
        return 0;
    }
}

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getProgress(JNIEnv* env, jobject thiz, 
                                                      jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        LOGE("Interpreter handle is null in getProgress");
        return 0.0;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        return GetProgress(interpreter);
    } catch (const std::exception& e) {
        LOGE("GetProgress failed: %s", e.what());
        return 0.0;
    } catch (...) {
        LOGE("GetProgress failed: unknown exception");
        return 0.0;
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLastInterpreterError(JNIEnv* env, jobject thiz, 
                                                                  jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        return env->NewStringUTF("");
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        const char* error = GetLastInterpreterError(interpreter);
        return toJavaString(env, error);
    } catch (const std::exception& e) {
        LOGE("GetLastInterpreterError failed: %s", e.what());
        return env->NewStringUTF("");
    } catch (...) {
        LOGE("GetLastInterpreterError failed: unknown exception");
        return env->NewStringUTF("");
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getError(JNIEnv* env, jobject thiz, 
                                                   jlong interpreterPtr, jint index) {
    if (interpreterPtr == 0) {
        return env->NewStringUTF("");
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        const char* error = GetError(interpreter, index);
        return toJavaString(env, error);
    } catch (const std::exception& e) {
        LOGE("GetError failed: %s", e.what());
        return env->NewStringUTF("");
    } catch (...) {
        LOGE("GetError failed: unknown exception");
        return env->NewStringUTF("");
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLogCount(JNIEnv* env, jobject thiz, 
                                                      jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        return 0;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        return GetLogCount(interpreter);
    } catch (const std::exception& e) {
        LOGE("GetLogCount failed: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("GetLogCount failed: unknown exception");
        return 0;
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLogEntry(JNIEnv* env, jobject thiz, 
                                                      jlong interpreterPtr, jint index) {
    if (interpreterPtr == 0) {
        return env->NewStringUTF("");
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        const char* log = GetLogEntry(interpreter, index);
        return toJavaString(env, log);
    } catch (const std::exception& e) {
        LOGE("GetLogEntry failed: %s", e.what());
        return env->NewStringUTF("");
    } catch (...) {
        LOGE("GetLogEntry failed: unknown exception");
        return env->NewStringUTF("");
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_clearLog(JNIEnv* env, jobject thiz, 
                                                   jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        return;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        ClearLog(interpreter);
        LOGI("Interpreter log cleared");
    } catch (const std::exception& e) {
        LOGE("ClearLog failed: %s", e.what());
    } catch (...) {
        LOGE("ClearLog failed: unknown exception");
    }
}

// 6. Методы конфигурации
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_readConfig(JNIEnv* env, jobject thiz, 
                                                     jlong interpreterPtr, jstring filename) {
    if (interpreterPtr == 0) {
        return JNI_FALSE;
    }
    
    if (!filename) {
        LOGE("Config filename is null!");
        return JNI_FALSE;
    }
    
    const char* c_filename = env->GetStringUTFChars(filename, nullptr);
    if (!c_filename) {
        LOGE("Failed to get config filename string");
        return JNI_FALSE;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        LOGI("Reading config: %s", c_filename);
        bool result = ReadConfig(interpreter, c_filename);
        
        env->ReleaseStringUTFChars(filename, c_filename);
        return result ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("ReadConfig failed: %s", e.what());
        env->ReleaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    } catch (...) {
        LOGE("ReadConfig failed: unknown exception");
        env->ReleaseStringUTFChars(filename, c_filename);
        return JNI_FALSE;
    }
}

// 7. Методы категорий логов
JNIEXPORT void JNICALL
Java_com_example_lpstudio_GCodeInterpreter_setLogCategories(JNIEnv* env, jobject thiz, 
                                                           jlong interpreterPtr, jint categories) {
    if (interpreterPtr == 0) {
        return;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        SetLogCategories(interpreter, static_cast<unsigned int>(categories));
        LOGI("Interpreter log categories set to: %u", categories);
    } catch (const std::exception& e) {
        LOGE("SetLogCategories failed: %s", e.what());
    } catch (...) {
        LOGE("SetLogCategories failed: unknown exception");
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getLogCategories(JNIEnv* env, jobject thiz, 
                                                           jlong interpreterPtr) {
    if (interpreterPtr == 0) {
        return 0;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        return static_cast<jint>(GetLogCategories(interpreter));
    } catch (const std::exception& e) {
        LOGE("GetLogCategories failed: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("GetLogCategories failed: unknown exception");
        return 0;
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getFilterLogCount(JNIEnv* env, jobject thiz, 
                                                            jlong interpreterPtr, jint categoryMask) {
    if (interpreterPtr == 0) {
        return 0;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        return GetFilterLogCount(interpreter, static_cast<unsigned int>(categoryMask));
    } catch (const std::exception& e) {
        LOGE("GetFilterLogCount failed: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("GetFilterLogCount failed: unknown exception");
        return 0;
    }
}

// Дополнительные методы для отладки

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_GCodeInterpreter_getInterpreterVersion(JNIEnv* env, jobject thiz) {
    const char* version = "GCode Interpreter JNI v1.0.0";
    return env->NewStringUTF(version);
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_GCodeInterpreter_checkInterpreterValid(JNIEnv* env, jobject thiz, 
                                                                jlong interpreterPtr) {
    if (!IsJNIInitialized()) {
        return JNI_FALSE;
    }
    
    if (interpreterPtr == 0) {
        return JNI_FALSE;
    }
    
    try {
        InterpreterHandle interpreter = toInterpreterHandle(interpreterPtr);
        // Простая проверка - пытаемся получить статус
        int status = GetStatus(interpreter);
        return (status >= 0) ? JNI_TRUE : JNI_FALSE;
    } catch (...) {
        return JNI_FALSE;
    }
}

#ifdef __cplusplus
}
#endif
