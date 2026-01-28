// printer_jni.h
#pragma once

#include <jni.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <android/log.h>
#include "jni_globals.h"  // Подключаем глобалы

#define LOG_TAG "PrinterJNI"
#ifdef DEBUG
#define JNI_LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define JNI_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define JNI_LOGI(...) ((void)0)
#define JNI_LOGD(...) ((void)0)
#endif
#define JNI_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Внешние зависимости
extern "C" {
    #include "LegoPrinterCore.h"
}

extern "C" {
  #include "interpreter.h"
}

// ========== Printer Runtime ==========
struct PrinterRuntime {
    IPrinter* printer;
    std::mutex mutex;
    std::atomic<bool> alive{false};
    
    PrinterRuntime() : printer(nullptr) {}
    ~PrinterRuntime();
};

PrinterRuntime* createPrinterRuntime();
void destroyPrinterRuntime(PrinterRuntime* runtime);
bool isPrinterAlive(PrinterRuntime* runtime);

// ========== Interpreter Runtime ==========
struct InterpreterRuntime {
    InterpreterHandle interpreter;
    std::mutex mutex;
    std::atomic<bool> alive{false};
    
    InterpreterRuntime() : interpreter(nullptr) {}
    ~InterpreterRuntime();
};

InterpreterRuntime* createInterpreterRuntime();
void destroyInterpreterRuntime(InterpreterRuntime* runtime);
bool isInterpreterAlive(InterpreterRuntime* runtime);

// ========== Safe Execution Wrappers ==========
template<typename Func>
bool executeWithPrinter(PrinterRuntime* rt, Func&& fn) {
    if (!rt) return false;
    
    std::lock_guard<std::mutex> lock(rt->mutex);
    if (!rt->alive || !rt->printer) return false;
    
    fn(rt->printer);
    return true;
}

template<typename Func>
bool executeWithInterpreter(InterpreterRuntime* rt, Func&& fn) {
    if (!rt) return false;
    
    std::lock_guard<std::mutex> lock(rt->mutex);
    if (!rt->alive || !rt->interpreter) return false;
    
    fn(rt->interpreter);
    return true;
}

// ========== Макросы для сокращения boilerplate ==========
#define WITH_PRINTER(handle, body) \
    executeWithPrinter(reinterpret_cast<PrinterRuntime*>(handle), [&](IPrinter* printer) body)

#define WITH_INTERPRETER(handle, body) \
    executeWithInterpreter(reinterpret_cast<InterpreterRuntime*>(handle), [&](InterpreterHandle interpreter) body)

// Правило порядка блокировок:
// ВСЕГДА: Interpreter → Printer
// НИКОГДА: Printer → Interpreter (чтобы избежать deadlock)
#define WITH_INTERPRETER_AND_PRINTER(interpreterHandle, printerHandle, body) \
    WITH_INTERPRETER(interpreterHandle, { \
        WITH_PRINTER(printerHandle, body); \
    })

// ========== Conversion Helpers ==========
struct SpeedProfileCore {
    jbyte port;
    std::vector<SpeedProfilePoint> points;
    jint timeoutMs;
    
    // Конвертация в формат Core
    SpeedProfile toCoreStruct() const {
        SpeedProfile result;
        result.port = port;
        result.timeoutMs = timeoutMs;
        result.count = static_cast<int>(points.size());
        
        if (!points.empty()) {
            result.points = new SpeedProfilePoint[result.count];
            std::copy(points.begin(), points.end(), result.points);
        } else {
            result.points = nullptr;
        }
        
        return result;
    }
};

// Прототипы функций конвертации
std::vector<MotorCommand> convertMotorCommands(JNIEnv* env, jobjectArray commandsArray, jint count);
std::vector<SpeedProfileCore> convertSpeedProfilesToCore(JNIEnv* env, jobjectArray profilesArray, jint count);
SpeedProfileCore convertSpeedProfile(JNIEnv* env, jobject jProfile);
SpeedProfilePoint convertSpeedProfilePoint(JNIEnv* env, jobject jPoint);
MotorCommand convertMotorCommand(JNIEnv* env, jobject jCmd);
