#include <jni.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "PrinterJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Включаем заголовки библиотек
extern "C" {
    #include "LegoPrinterCore.h"
}

// Объявляем внешние функции
extern "C" JNIEnv* GetJNIEnv();
extern "C" jboolean IsJNIInitialized();

// ========== Вспомогательные функции ==========

// Конвертация MotorCommand
void convertMotorCommand(JNIEnv* env, jobject jCmd, MotorCommand* cmd) {
    if (!env || !jCmd || !cmd) {
        LOGE("convertMotorCommand: invalid parameters");
        return;
    }
    
    jclass cls = env->GetObjectClass(jCmd);
    if (!cls) {
        LOGE("convertMotorCommand: failed to get class");
        return;
    }
    
    jfieldID portField = env->GetFieldID(cls, "port", "B");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID revolutionsField = env->GetFieldID(cls, "revolutions", "D");
    
    if (!portField || !speedField || !revolutionsField) {
        LOGE("convertMotorCommand: failed to get field IDs");
        env->DeleteLocalRef(cls);
        return;
    }
    
    cmd->port = env->GetByteField(jCmd, portField);
    cmd->speed = env->GetByteField(jCmd, speedField);
    cmd->revolutions = env->GetDoubleField(jCmd, revolutionsField);
    
    env->DeleteLocalRef(cls);
}

// Конвертация SpeedProfilePoint
void convertSpeedProfilePoint(JNIEnv* env, jobject jPoint, SpeedProfilePoint* point) {
    if (!env || !jPoint || !point) {
        LOGE("convertSpeedProfilePoint: invalid parameters");
        return;
    }
    
    // Используем полное имя класса
    jclass cls = env->FindClass("com/example/lpstudio/PrinterController$SpeedProfilePoint");
    if (!cls) {
        LOGE("convertSpeedProfilePoint: class not found");
        return;
    }
    
    jfieldID distanceField = env->GetFieldID(cls, "distance", "D");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID toleranceField = env->GetFieldID(cls, "tolerance", "D");
    
    if (!distanceField || !speedField || !toleranceField) {
        LOGE("convertSpeedProfilePoint: failed to get field IDs");
        env->DeleteLocalRef(cls);
        return;
    }
    
    point->distance = env->GetDoubleField(jPoint, distanceField);
    point->speed = env->GetByteField(jPoint, speedField);
    point->tolerance = env->GetDoubleField(jPoint, toleranceField);
    
    env->DeleteLocalRef(cls);
}

// Конвертация SpeedProfile
SpeedProfile* convertSpeedProfile(JNIEnv* env, jobject jProfile) {
    if (!env || !jProfile) {
        LOGE("convertSpeedProfile: invalid parameters");
        return nullptr;
    }
    
    jclass profileCls = env->FindClass("com/example/lpstudio/PrinterController$SpeedProfile");
    if (!profileCls) {
        LOGE("convertSpeedProfile: class not found");
        return nullptr;
    }
    
    jfieldID portField = env->GetFieldID(profileCls, "port", "B");
    jfieldID pointsField = env->GetFieldID(profileCls, "points", 
        "[Lcom/example/lpstudio/PrinterController$SpeedProfilePoint;");
    jfieldID timeoutField = env->GetFieldID(profileCls, "timeoutMs", "I");
    
    if (!portField || !pointsField || !timeoutField) {
        LOGE("convertSpeedProfile: failed to get field IDs");
        env->DeleteLocalRef(profileCls);
        return nullptr;
    }
    
    SpeedProfile* profile = new SpeedProfile();
    profile->port = env->GetByteField(jProfile, portField);
    profile->timeoutMs = env->GetIntField(jProfile, timeoutField);
    
    jobjectArray pointsArray = (jobjectArray)env->GetObjectField(jProfile, pointsField);
    profile->count = env->GetArrayLength(pointsArray);
    
    if (profile->count > 0) {
        profile->points = new SpeedProfilePoint[profile->count];
        for (int i = 0; i < profile->count; i++) {
            jobject jPoint = env->GetObjectArrayElement(pointsArray, i);
            convertSpeedProfilePoint(env, jPoint, &profile->points[i]);
            env->DeleteLocalRef(jPoint);
        }
    } else {
        profile->points = nullptr;
    }
    
    env->DeleteLocalRef(pointsArray);
    env->DeleteLocalRef(profileCls);
    return profile;
}

// Освобождение SpeedProfile
void freeSpeedProfile(SpeedProfile* profile) {
    if (profile) {
        if (profile->points) {
            delete[] profile->points;
        }
        delete profile;
    }
}

// Проверка инициализации JNI перед вызовом нативных методов
bool CheckJNIAndPrinter(JNIEnv* env, jlong printerPtr) {
    if (!IsJNIInitialized()) {  // Теперь это работает правильно
        LOGE("JNI not initialized!");
        return false;
    }
    
    if (printerPtr == 0) {
        LOGE("Printer pointer is null!");
        return false;
    }
    
    return true;
}


// =========== JNI методы ==========

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_PrinterController_createPrinter(JNIEnv* env, jobject /* this */) {
    LOGI("createPrinter called");
    
    if (!IsJNIInitialized()) {
        LOGE("JNI not initialized in createPrinter!");
        return 0;
    }
    
    IPrinter* printer = CreatePrinter();
    if (!printer) {
        LOGE("CreatePrinter returned null!");
        return 0;
    }
    
    LOGI("Printer created: %p", printer);
    return reinterpret_cast<jlong>(printer);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_destroyPrinter(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    LOGI("Destroying printer: %p", printer);
    DestroyPrinter(printer);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerConnect(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    LOGI("Connecting printer: %p", printer);
    
    bool result = PrinterConnect(printer);
    LOGI("PrinterConnect result: %s", result ? "true" : "false");
    
    if (!result) {
        const char* error = GetLastErrorMessage(printer);
        LOGE("Connection failed: %s", error ? error : "Unknown error");
    }
    
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerDisconnect(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    LOGI("Disconnecting printer: %p", printer);
    
    bool result = PrinterDisconnect(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_isConnected(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    bool result = IsConnected(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerRotateMotor(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jobjectArray commandsArray, jint count) {
    
    if (!IsJNIInitialized()) {
        LOGE("JNI not initialized!");
        return;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !commandsArray || count <= 0) {
        LOGE("Invalid parameters for printerRotateMotor");
        return;
    }
    
    std::vector<MotorCommand> commands(count);
    
    for (jint i = 0; i < count; i++) {
        jobject jCmd = env->GetObjectArrayElement(commandsArray, i);
        if (jCmd) {
            convertMotorCommand(env, jCmd, &commands[i]);
            env->DeleteLocalRef(jCmd);
        }
    }
    
    LOGI("Rotating %d motors", count);
    PrinterRotateMotor(printer, commands.data(), count);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSendCommand(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jbyteArray commandArray, jint length) {
    
    if (!IsJNIInitialized()) {
        LOGE("JNI not initialized!");
        return;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !commandArray || length <= 0) {
        LOGE("Invalid parameters for printerSendCommand");
        return;
    }
    
    jbyte* bytes = env->GetByteArrayElements(commandArray, nullptr);
    if (!bytes) {
        LOGE("Failed to get byte array elements");
        return;
    }
    
    LOGI("Sending command of length: %d", length);
    PrinterSendCommand(printer, reinterpret_cast<const unsigned char*>(bytes), length);
    
    env->ReleaseByteArrayElements(commandArray, bytes, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetMotorSpeed(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jbyte port, jbyte speed) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    LOGI("Setting motor speed: port=%d, speed=%d", port, speed);
    PrinterSetMotorSpeed(printer, port, speed);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_getLogCount(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return 0;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    return GetLogCount(printer);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLogEntry(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jint index) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return env->NewStringUTF("");
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    const char* entry = GetLogEntry(printer, index);
    return entry ? env->NewStringUTF(entry) : env->NewStringUTF("");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_clearLog(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    ClearLog(printer);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLastErrorMessage(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return env->NewStringUTF("JNI not initialized or printer is null");
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    const char* error = GetLastErrorMessage(printer);
    return error ? env->NewStringUTF(error) : env->NewStringUTF("");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_printerConnectionInfo(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return env->NewStringUTF("");
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    PrinterConnectionInfo(printer);
    
    // Возвращаем последнюю запись лога
    int count = GetLogCount(printer);
    if (count > 0) {
        const char* lastLog = GetLogEntry(printer, count - 1);
        if (lastLog) {
            return env->NewStringUTF(lastLog);
        }
    }
    
    return env->NewStringUTF("");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetLogCategories(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jint categories) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    PrinterSetLogCategories(printer, categories);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_printerGetLogCategories(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return 0;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    return PrinterGetLogCategories(printer);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfile(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jobject jProfile) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!jProfile) {
        LOGE("SpeedProfile is null");
        return JNI_FALSE;
    }
    
    SpeedProfile* profile = convertSpeedProfile(env, jProfile);
    if (!profile) {
        LOGE("Failed to convert SpeedProfile");
        return JNI_FALSE;
    }
    
    LOGI("Executing speed profile with %d points", profile->count);
    bool result = PrinterExecuteSpeedProfile(printer, profile);
    
    freeSpeedProfile(profile);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfiles(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jobjectArray profilesArray, jint count) {
    
    if (!IsJNIInitialized()) {
        LOGE("JNI not initialized!");
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !profilesArray || count <= 0) {
        LOGE("Invalid parameters for printerExecuteSpeedProfiles");
        return JNI_FALSE;
    }
    
    std::vector<SpeedProfile*> profiles;
    std::vector<SpeedProfile> profileArray;
    
    try {
        for (jint i = 0; i < count; i++) {
            jobject jProfile = env->GetObjectArrayElement(profilesArray, i);
            if (jProfile) {
                SpeedProfile* profile = convertSpeedProfile(env, jProfile);
                if (profile) {
                    profiles.push_back(profile);
                    profileArray.push_back(*profile);
                }
                env->DeleteLocalRef(jProfile);
            }
        }
        
        if (profileArray.empty()) {
            LOGE("No valid profiles to execute");
            return JNI_FALSE;
        }
        
        LOGI("Executing %d speed profiles", profileArray.size());
        bool result = PrinterExecuteSpeedProfiles(printer, profileArray.data(), profileArray.size());
        
        // Очистка
        for (SpeedProfile* profile : profiles) {
            freeSpeedProfile(profile);
        }
        
        return result ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        LOGE("Exception in printerExecuteSpeedProfiles: %s", e.what());
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsMotorMoving(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    bool result = PrinterIsMotorMoving(printer, 1); // Проверяем первый мотор
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_PrinterController_printerGetMotorPosition(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jbyte port) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return 0.0;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    return PrinterGetMotorPosition(printer, port);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_runPrinterTest(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jstring testName) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!testName) {
        LOGE("Test name is null");
        return JNI_FALSE;
    }
    
    const char* cTestName = env->GetStringUTFChars(testName, nullptr);
    if (!cTestName) {
        LOGE("Failed to get test name string");
        return JNI_FALSE;
    }
    
    LOGI("Running printer test: %s", cTestName);
    bool result = RunPrinterTest(printer, cTestName);
    
    env->ReleaseStringUTFChars(testName, cTestName);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerRequestBatteryLevel(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    bool result = PrinterRequestBatteryLevel(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jbyte JNICALL
Java_com_example_lpstudio_PrinterController_printerGetBatteryLevel(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return 0;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    return PrinterGetBatteryLevel(printer);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsBatteryLevelFresh(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jint maxAgeSeconds) {
    
    if (!CheckJNIAndPrinter(env, printerPtr)) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    bool result = PrinterIsBatteryLevelFresh(printer, maxAgeSeconds);
    return result ? JNI_TRUE : JNI_FALSE;
}

// Дополнительные функции для отладки

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getNativeVersion(
    JNIEnv* env, jobject /* this */) {
    const char* version = "Printer JNI v1.0.0";
    return env->NewStringUTF(version);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_checkPrinterValid(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    if (!IsJNIInitialized()) {
        return JNI_FALSE;
    }
    
    if (printerPtr == 0) {
        return JNI_FALSE;
    }
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    // Простая проверка - пытаемся получить лог
    int logCount = GetLogCount(printer);
    return (logCount >= 0) ? JNI_TRUE : JNI_FALSE;
}
