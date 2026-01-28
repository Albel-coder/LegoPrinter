// printer_runtime.cpp
#include "printer_jni.h"

// ========== Printer Runtime Implementation ==========
PrinterRuntime::~PrinterRuntime() {
    alive = false;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    if (printer) {
        DestroyPrinter(printer);
        printer = nullptr;
    }
}

PrinterRuntime* createPrinterRuntime() {
    auto* runtime = new PrinterRuntime();
    
    runtime->printer = CreatePrinter();
    if (!runtime->printer) {
        JNI_LOGE("CreatePrinter returned null!");
        delete runtime;
        return nullptr;
    }
    
    runtime->alive = true;
    JNI_LOGI("PrinterRuntime created: %p", runtime);
    return runtime;
}

void destroyPrinterRuntime(PrinterRuntime* runtime) {
    if (!runtime) return;
    
    // Устанавливаем флаг "неживой"
    runtime->alive = false;
    
    // Блокируем мьютекс, чтобы гарантировать, что активных вызовов нет
    std::lock_guard<std::mutex> lock(runtime->mutex);
    
    if (runtime->printer) {
        DestroyPrinter(runtime->printer);
        runtime->printer = nullptr;
    }
    
    delete runtime;
    JNI_LOGI("PrinterRuntime destroyed");
}

bool isPrinterAlive(PrinterRuntime* runtime) {
    return runtime && runtime->alive.load() && runtime->printer;
}

// ========== Interpreter Runtime Implementation ==========
InterpreterRuntime::~InterpreterRuntime() {
    alive = false;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    if (interpreter) {
        DestroyInterpreter(interpreter);
        interpreter = nullptr;
    }
}

InterpreterRuntime* createInterpreterRuntime() {
    auto* runtime = new InterpreterRuntime();
    
    runtime->interpreter = CreateInterpreter();
    runtime->alive = (runtime->interpreter != nullptr);
    
    if (!runtime->alive) {
        JNI_LOGE("CreateInterpreter returned null!");
        delete runtime;
        return nullptr;
    }
    
    JNI_LOGI("InterpreterRuntime created: %p", runtime);
    return runtime;
}

void destroyInterpreterRuntime(InterpreterRuntime* runtime) {
    if (!runtime) return;
    
    runtime->alive = false;
    
    std::lock_guard<std::mutex> lock(runtime->mutex);
    
    if (runtime->interpreter) {
        DestroyInterpreter(runtime->interpreter);
        runtime->interpreter = nullptr;
    }
    
    delete runtime;
    JNI_LOGI("InterpreterRuntime destroyed");
}

bool isInterpreterAlive(InterpreterRuntime* runtime) {
    return runtime && runtime->alive.load() && runtime->interpreter;
}
