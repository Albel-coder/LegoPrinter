#include "printer_jni.h"
#include "jni_safe_call.h"
#include <vector>

extern "C" {

// ========== Lifecycle Management ==========
JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_PrinterController_createPrinter(JNIEnv*, jobject) {
    JNISafeCall guard;
    if (!guard.valid()) {
        JNI_LOGE("JNI not initialized in createPrinter");
        return 0;
    }
    
    try {
        PrinterRuntime* runtime = createPrinterRuntime();
        if (runtime) {
            JNI_LOGI("PrinterRuntime created successfully: %p", runtime);
            return reinterpret_cast<jlong>(runtime);
        } else {
            JNI_LOGE("Failed to create PrinterRuntime");
            return 0;
        }
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in createPrinter: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in createPrinter");
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_destroyPrinter(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (runtimePtr == 0) {
        JNI_LOGE("Invalid runtime handle in destroyPrinter");
        return;
    }
    
    try {
        destroyPrinterRuntime(reinterpret_cast<PrinterRuntime*>(runtimePtr));
        JNI_LOGI("PrinterRuntime destroyed successfully");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in destroyPrinter: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in destroyPrinter");
    }
}

// ========== Connection Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerConnect(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerConnect");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterConnect(printer);
            JNI_LOGI("PrinterConnect result: %s", result ? "true" : "false");
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerConnect: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerConnect");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerDisconnect(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerDisconnect");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterDisconnect(printer);
            JNI_LOGI("PrinterDisconnect result: %s", result ? "true" : "false");
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerDisconnect: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerDisconnect");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_isConnected(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in isConnected");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        WITH_PRINTER(runtimePtr, {
            result = IsConnected(printer);
            JNI_LOGD("IsConnected result: %s", result ? "true" : "false");
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in isConnected: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in isConnected");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Motor Control Methods ==========
JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerRotateMotor(
    JNIEnv* env, jobject, jlong runtimePtr, 
    jobjectArray commandsArray, jint count) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0 || !commandsArray || count <= 0) {
        JNI_LOGE("Invalid parameters in printerRotateMotor");
        return;
    }
    
    try {
        auto commands = convertMotorCommands(guard.env(), commandsArray, count);
        if (commands.empty()) {
            JNI_LOGE("No commands to execute in printerRotateMotor");
            return;
        }
        
        JNI_LOGI("Executing rotate motor with %zu commands", commands.size());
        WITH_PRINTER(runtimePtr, {
            PrinterRotateMotor(printer, commands.data(), static_cast<int>(commands.size()));
        });
        JNI_LOGI("Rotate motor command executed successfully");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerRotateMotor: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in printerRotateMotor");
    }
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSendCommand(
    JNIEnv* env, jobject, jlong runtimePtr, 
    jbyteArray commandArray, jint length) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0 || !commandArray || length <= 0) {
        JNI_LOGE("Invalid parameters in printerSendCommand");
        return;
    }
    
    jbyte* bytes = guard.getByteArrayElements(commandArray, nullptr);
    if (!bytes) {
        JNI_LOGE("Failed to get byte array in printerSendCommand");
        return;
    }
    
    try {
        JNI_LOGI("Sending command of length %d", length);
        WITH_PRINTER(runtimePtr, {
            PrinterSendCommand(printer, reinterpret_cast<const unsigned char*>(bytes), length);
        });
        JNI_LOGI("Command sent successfully");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerSendCommand: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in printerSendCommand");
    }
    
    guard.releaseByteArrayElements(commandArray, bytes, 0);
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetMotorSpeed(
    JNIEnv*, jobject, jlong runtimePtr, jbyte port, jbyte speed) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerSetMotorSpeed");
        return;
    }
    
    try {
        JNI_LOGI("Setting motor speed: port=%d, speed=%d", port, speed);
        WITH_PRINTER(runtimePtr, {
            PrinterSetMotorSpeed(printer, port, speed);
        });
        JNI_LOGI("Motor speed set successfully");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerSetMotorSpeed: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in printerSetMotorSpeed");
    }
}

// ========== Logging Methods ==========
JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_getLogCount(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in getLogCount");
        return 0;
    }
    
    jint result = 0;
    try {
        WITH_PRINTER(runtimePtr, {
            result = GetLogCount(printer);
        });
        JNI_LOGD("Log count: %d", result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getLogCount: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in getLogCount");
        return 0;
    }
    
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLogEntry(
    JNIEnv* env, jobject, jlong runtimePtr, jint index) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in getLogEntry");
        return guard.valid() ? guard.env()->NewStringUTF("") : nullptr;
    }
    
    const char* entry = nullptr;
    try {
        WITH_PRINTER(runtimePtr, {
            entry = GetLogEntry(printer, index);
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getLogEntry: %s", e.what());
        return guard.env()->NewStringUTF("");
    } catch (...) {
        JNI_LOGE("Unknown exception in getLogEntry");
        return guard.env()->NewStringUTF("");
    }
    
    return entry ? guard.env()->NewStringUTF(entry) : guard.env()->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_clearLog(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in clearLog");
        return;
    }
    
    try {
        WITH_PRINTER(runtimePtr, {
            ClearLog(printer);
        });
        JNI_LOGI("Log cleared successfully");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in clearLog: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in clearLog");
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLastErrorMessage(
    JNIEnv* env, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in getLastErrorMessage");
        return guard.valid() ? guard.env()->NewStringUTF("") : nullptr;
    }
    
    const char* error = nullptr;
    try {
        WITH_PRINTER(runtimePtr, {
            error = GetLastErrorMessage(printer);
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in getLastErrorMessage: %s", e.what());
        return guard.env()->NewStringUTF("");
    } catch (...) {
        JNI_LOGE("Unknown exception in getLastErrorMessage");
        return guard.env()->NewStringUTF("");
    }
    
    return error ? guard.env()->NewStringUTF(error) : guard.env()->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_printerConnectionInfo(
    JNIEnv* env, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerConnectionInfo");
        return guard.valid() ? guard.env()->NewStringUTF("") : nullptr;
    }
    
    const char* info = nullptr;
    try {
        WITH_PRINTER(runtimePtr, {
            PrinterConnectionInfo(printer);
            int count = GetLogCount(printer);
            if (count > 0) {
                info = GetLogEntry(printer, count - 1);
            }
        });
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerConnectionInfo: %s", e.what());
        return guard.env()->NewStringUTF("");
    } catch (...) {
        JNI_LOGE("Unknown exception in printerConnectionInfo");
        return guard.env()->NewStringUTF("");
    }
    
    return info ? guard.env()->NewStringUTF(info) : guard.env()->NewStringUTF("");
}

// ========== Log Categories Methods ==========
JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetLogCategories(
    JNIEnv*, jobject, jlong runtimePtr, jint categories) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerSetLogCategories");
        return;
    }
    
    try {
        WITH_PRINTER(runtimePtr, {
            PrinterSetLogCategories(printer, categories);
        });
        JNI_LOGI("Log categories set to: 0x%X", categories);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerSetLogCategories: %s", e.what());
    } catch (...) {
        JNI_LOGE("Unknown exception in printerSetLogCategories");
    }
}

JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_printerGetLogCategories(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerGetLogCategories");
        return 0;
    }
    
    jint result = 0;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterGetLogCategories(printer);
        });
        JNI_LOGD("Current log categories: 0x%X", result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerGetLogCategories: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerGetLogCategories");
        return 0;
    }
    
    return result;
}

// ========== Speed Profile Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfile(
    JNIEnv* env, jobject, jlong runtimePtr, jobject jProfile) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0 || !jProfile) {
        JNI_LOGE("Invalid parameters in printerExecuteSpeedProfile");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        auto profileCore = convertSpeedProfile(guard.env(), jProfile);
        SpeedProfile coreStruct = profileCore.toCoreStruct();
        
        JNI_LOGI("Executing speed profile for port %d with %d points", 
                profileCore.port, coreStruct.count);
        
        WITH_PRINTER(runtimePtr, {
            result = PrinterExecuteSpeedProfile(printer, &coreStruct);
        });
        
        JNI_LOGI("Speed profile execution result: %s", result ? "true" : "false");
        
        // Освобождаем выделенную память
        if (coreStruct.points) {
            delete[] coreStruct.points;
        }
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerExecuteSpeedProfile: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerExecuteSpeedProfile");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfiles(
    JNIEnv* env, jobject, jlong runtimePtr, 
    jobjectArray profilesArray, jint count) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0 || !profilesArray || count <= 0) {
        JNI_LOGE("Invalid parameters in printerExecuteSpeedProfiles");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        auto profiles = convertSpeedProfilesToCore(guard.env(), profilesArray, count);
        if (profiles.empty()) {
            JNI_LOGE("No profiles to execute in printerExecuteSpeedProfiles");
            return JNI_FALSE;
        }
        
        // Конвертируем в структуры Core
        std::vector<SpeedProfile> coreStructs;
        coreStructs.reserve(profiles.size());
        
        for (const auto& profile : profiles) {
            coreStructs.push_back(profile.toCoreStruct());
        }
        
        JNI_LOGI("Executing %zu speed profiles", coreStructs.size());
        
        WITH_PRINTER(runtimePtr, {
            result = PrinterExecuteSpeedProfiles(printer, coreStructs.data(), static_cast<int>(coreStructs.size()));
        });
        
        JNI_LOGI("Speed profiles execution result: %s", result ? "true" : "false");
        
        // Освобождаем выделенную память
        for (auto& coreStruct : coreStructs) {
            if (coreStruct.points) {
                delete[] coreStruct.points;
            }
        }
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerExecuteSpeedProfiles: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerExecuteSpeedProfiles");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Monitoring Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsMotorMoving(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerIsMotorMoving");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterIsMotorMoving(printer, 1); // Проверяем первый мотор
        });
        JNI_LOGD("Motor moving: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerIsMotorMoving: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerIsMotorMoving");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_PrinterController_printerGetMotorPosition(
    JNIEnv*, jobject, jlong runtimePtr, jbyte port) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerGetMotorPosition");
        return 0.0;
    }
    
    jdouble result = 0.0;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterGetMotorPosition(printer, port);
        });
        JNI_LOGD("Motor position for port %d: %f", port, result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerGetMotorPosition: %s", e.what());
        return 0.0;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerGetMotorPosition");
        return 0.0;
    }
    
    return result;
}

// ========== Testing Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_runPrinterTest(
    JNIEnv* env, jobject, jlong runtimePtr, jstring testName) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in runPrinterTest");
        return JNI_FALSE;
    }
    
    const char* cTestName = guard.getStringUTFChars(testName, nullptr);
    if (!cTestName) return JNI_FALSE;
    
    bool result = false;
    try {
        JNI_LOGI("Running printer test: %s", cTestName);
        WITH_PRINTER(runtimePtr, {
            result = RunPrinterTest(printer, cTestName);
        });
        JNI_LOGI("Printer test result: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in runPrinterTest: %s", e.what());
        guard.releaseStringUTFChars(testName, cTestName);
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in runPrinterTest");
        guard.releaseStringUTFChars(testName, cTestName);
        return JNI_FALSE;
    }
    
    guard.releaseStringUTFChars(testName, cTestName);
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Battery Methods ==========
JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerRequestBatteryLevel(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerRequestBatteryLevel");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterRequestBatteryLevel(printer);
        });
        JNI_LOGI("Battery level request result: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerRequestBatteryLevel: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerRequestBatteryLevel");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jbyte JNICALL
Java_com_example_lpstudio_PrinterController_printerGetBatteryLevel(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerGetBatteryLevel");
        return 0;
    }
    
    jbyte result = 0;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterGetBatteryLevel(printer);
        });
        JNI_LOGD("Battery level: %d", result);
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerGetBatteryLevel: %s", e.what());
        return 0;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerGetBatteryLevel");
        return 0;
    }
    
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsBatteryLevelFresh(
    JNIEnv*, jobject, jlong runtimePtr, jint maxAgeSeconds) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in printerIsBatteryLevelFresh");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        WITH_PRINTER(runtimePtr, {
            result = PrinterIsBatteryLevelFresh(printer, maxAgeSeconds);
        });
        JNI_LOGD("Battery level fresh (max age %ds): %s", 
                maxAgeSeconds, result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in printerIsBatteryLevelFresh: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in printerIsBatteryLevelFresh");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

// ========== Utility Methods ==========
JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getNativeVersion(JNIEnv* env, jobject) {
    JNISafeCall guard;
    if (!guard.valid()) {
        JNI_LOGE("JNI not initialized in getNativeVersion");
        return nullptr;
    }
    
    const char* version = "Printer JNI v2.1 (SafeCall)";
    return guard.env()->NewStringUTF(version);
}

JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_checkPrinterValid(
    JNIEnv*, jobject, jlong runtimePtr) {
    
    JNISafeCall guard;
    if (!guard.valid() || runtimePtr == 0) {
        JNI_LOGE("Invalid parameters in checkPrinterValid");
        return JNI_FALSE;
    }
    
    bool result = false;
    try {
        auto* runtime = reinterpret_cast<PrinterRuntime*>(runtimePtr);
        std::lock_guard<std::mutex> lock(runtime->mutex);
        result = (runtime->alive && runtime->printer);
        JNI_LOGD("Printer valid check: %s", result ? "true" : "false");
    } catch (const std::exception& e) {
        JNI_LOGE("Exception in checkPrinterValid: %s", e.what());
        return JNI_FALSE;
    } catch (...) {
        JNI_LOGE("Unknown exception in checkPrinterValid");
        return JNI_FALSE;
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

} // extern "C”
