#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "PrinterJNI"
#define LOGD(...) android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, VA_ARGS__)
#define LOGE(...) android_log_print(ANDROID_LOG_ERROR, LOG_TAG, VA_ARGS__)

// Включаем ваш header файл
extern "C" {
    #include "printer_driver.h"
}

// ========== Вспомогательные функции ==========

// Преобразование Java MotorCommand в C MotorCommand
void convertMotorCommand(JNIEnv* env, jobject jCmd, MotorCommand* cmd) {
    jclass cls = env->GetObjectClass(jCmd);
    
    jfieldID portField = env->GetFieldID(cls, "port", "B");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID revolutionsField = env->GetFieldID(cls, "revolutions", "D");
    
    cmd->port = env->GetByteField(jCmd, portField);
    cmd->speed = env->GetByteField(jCmd, speedField);
    cmd->revolutions = env->GetDoubleField(jCmd, revolutionsField);
    
    env->DeleteLocalRef(cls);
}

// Преобразование Java SpeedProfilePoint в C SpeedProfilePoint
void convertSpeedProfilePoint(JNIEnv* env, jobject jPoint, SpeedProfilePoint* point) {
    jclass cls = env->FindClass("com/example/printerapp/PrinterController$SpeedProfilePoint");
    
    jfieldID positionField = env->GetFieldID(cls, "position", "D");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID toleranceField = env->GetFieldID(cls, "tolerance", "D");
    
    point->position = env->GetDoubleField(jPoint, positionField);
    point->speed = env->GetByteField(jPoint, speedField);
    point->tolerance = env->GetDoubleField(jPoint, toleranceField);
    
    env->DeleteLocalRef(cls);
}

// Преобразование Java SpeedProfile в C SpeedProfile
SpeedProfile* convertSpeedProfile(JNIEnv* env, jobject jProfile) {
    jclass profileCls = env->FindClass("com/example/printerapp/PrinterController$SpeedProfile");
    
    jfieldID portField = env->GetFieldID(profileCls, "port", "B");
    jfieldID pointsField = env->GetFieldID(profileCls, "points", 
        "[Lcom/example/printerapp/PrinterController$SpeedProfilePoint;");
    jfieldID timeoutField = env->GetFieldID(profileCls, "timeoutMs", "I");
    
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

// Освобождение памяти SpeedProfile
void freeSpeedProfile(SpeedProfile* profile) {
    if (profile) {
        if (profile->points) {
            delete[] profile->points;
        }
        delete profile;
    }
}

// ========== JNI методы (чистые прокси) ==========

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_lpstudio_PrinterController_createPrinter(JNIEnv* env, jobject /* this */) {
    IPrinter* printer = CreatePrinter();
    LOGD("CreatePrinter: %p -> %lld", printer, (long long)printer);
    return reinterpret_cast<jlong>(printer);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_destroyPrinter(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (printer) {
        DestroyPrinter(printer);
        LOGD("DestroyPrinter: %p", printer);
    }
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerConnect(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) {
        LOGE("printerConnect: printer is null");
        return JNI_FALSE;
    }
    
    bool result = PrinterConnect(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerDisconnect(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return JNI_FALSE;
    
    bool result = PrinterDisconnect(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_isConnected(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return JNI_FALSE;
    
    bool result = IsConnected(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerRotateMotor(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jobjectArray commandsArray, jint count) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !commandsArray || count <= 0) return;
    
    MotorCommand* commands = new MotorCommand[count];
    
    for (jint i = 0; i < count; i++) {
        jobject jCmd = env->GetObjectArrayElement(commandsArray, i);
        convertMotorCommand(env, jCmd, &commands[i]);
        env->DeleteLocalRef(jCmd);
    }
    
    PrinterRotateMotor(printer, commands, count);
    delete[] commands;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSendCommand(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jbyteArray commandArray, jint length) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !commandArray || length <= 0) return;
    
    jbyte* bytes = env->GetByteArrayElements(commandArray, nullptr);
    if (bytes) {
        PrinterSendCommand(printer, reinterpret_cast<const unsigned char*>(bytes), length);
        env->ReleaseByteArrayElements(commandArray, bytes, 0);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_printerSetMotorSpeed(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jbyte port, jbyte speed) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return;
    
    PrinterSetMotorSpeed(printer, port, speed);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_getLogCount(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return 0;
    
    return GetLogCount(printer);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLogEntry(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jint index) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return env->NewStringUTF("");
    
    const char* entry = GetLogEntry(printer, index);
    if (!entry) return env->NewStringUTF("");
    
    return env->NewStringUTF(entry);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_lpstudio_PrinterController_clearLog(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (printer) ClearLog(printer);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_getLastErrorMessage(
	JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return env->NewStringUTF("Printer is null");
    
    const char* error = GetLastErrorMessage(printer);
    if (!error) return env->NewStringUTF("");
    
    return env->NewStringUTF(error);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lpstudio_PrinterController_printerConnectionInfo(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return env->NewStringUTF("");
    
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
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (printer) PrinterSetLogCategories(printer, categories);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_lpstudio_PrinterController_printerGetLogCategories(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return 0;
    
    return PrinterGetLogCategories(printer);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfile(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jobject jProfile) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !jProfile) return JNI_FALSE;
    
    SpeedProfile* profile = convertSpeedProfile(env, jProfile);
    bool result = PrinterExecuteSpeedProfile(printer, profile);
    freeSpeedProfile(profile);
    
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerExecuteSpeedProfiles(
    JNIEnv* env, jobject /* this */, jlong printerPtr, 
    jobjectArray profilesArray, jint count) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !profilesArray || count <= 0) return JNI_FALSE;
    
    SpeedProfile** profiles = new SpeedProfile*[count];
    for (jint i = 0; i < count; i++) {
        jobject jProfile = env->GetObjectArrayElement(profilesArray, i);
        profiles[i] = convertSpeedProfile(env, jProfile);
        env->DeleteLocalRef(jProfile);
    }
    
    // Создаем массив указателей
    SpeedProfile* profileArray = new SpeedProfile[count];
    for (int i = 0; i < count; i++) {
        profileArray[i] = *profiles[i];
    }
    
    bool result = PrinterExecuteSpeedProfiles(printer, profileArray, count);
    
    // Освобождаем память
    for (jint i = 0; i < count; i++) {
        freeSpeedProfile(profiles[i]);
    }
    delete[] profiles;
    delete[] profileArray;
    
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsMotorMoving(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return JNI_FALSE;
    
    // Если функция требует count, передаем 1
    bool result = PrinterIsMotorMoving(printer, 1);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_lpstudio_PrinterController_printerGetMotorPosition(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jbyte port) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return 0.0;
    
    return PrinterGetMotorPosition(printer, port);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_runPrinterTest(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jstring testName) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer || !testName) return JNI_FALSE;
    
    const char* cTestName = env->GetStringUTFChars(testName, nullptr);
    if (!cTestName) return JNI_FALSE;
    
    bool result = RunPrinterTest(printer, cTestName);
    env->ReleaseStringUTFChars(testName, cTestName);
    
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerRequestBatteryLevel(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return JNI_FALSE;
    
    bool result = PrinterRequestBatteryLevel(printer);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jbyte JNICALL
Java_com_example_lpstudio_PrinterController_printerGetBatteryLevel(
    JNIEnv* env, jobject /* this */, jlong printerPtr) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return 0;
    
    return PrinterGetBatteryLevel(printer);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_lpstudio_PrinterController_printerIsBatteryLevelFresh(
    JNIEnv* env, jobject /* this */, jlong printerPtr, jint maxAgeSeconds) {
    
    IPrinter* printer = reinterpret_cast<IPrinter*>(printerPtr);
    if (!printer) return JNI_FALSE;
    
    bool result = PrinterIsBatteryLevelFresh(printer, maxAgeSeconds);
    return result ? JNI_TRUE : JNI_FALSE;
}

// Обязательная функция для JNI
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGD("JNI_OnLoad called");
    return JNI_VERSION_1_6;
}