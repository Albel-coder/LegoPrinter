#include "printer_jni.h"
#include <vector>

// Вспомогательная функция для конвертации MotorCommand
MotorCommand convertMotorCommand(JNIEnv* env, jobject jCmd) {
    MotorCommand cmd{};
    
    jclass cls = env->GetObjectClass(jCmd);
    jfieldID portField = env->GetFieldID(cls, "port", "B");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID revolutionsField = env->GetFieldID(cls, "revolutions", "D");
    
    if (portField && speedField && revolutionsField) {
        cmd.port = env->GetByteField(jCmd, portField);
        cmd.speed = env->GetByteField(jCmd, speedField);
        cmd.revolutions = env->GetDoubleField(jCmd, revolutionsField);
    }
    
    env->DeleteLocalRef(cls);
    return cmd;
}

// Вспомогательная функция для конвертации SpeedProfilePoint
SpeedProfilePoint convertSpeedProfilePoint(JNIEnv* env, jobject jPoint) {
    SpeedProfilePoint point{};
    
    if (!g_SpeedProfilePointClass) return point;
    
    jfieldID distanceField = env->GetFieldID(g_SpeedProfilePointClass, "distance", "D");
    jfieldID speedField = env->GetFieldID(g_SpeedProfilePointClass, "speed", "B");
    jfieldID toleranceField = env->GetFieldID(g_SpeedProfilePointClass, "tolerance", "D");
    
    if (distanceField && speedField && toleranceField) {
        point.distance = env->GetDoubleField(jPoint, distanceField);
        point.speed = env->GetByteField(jPoint, speedField);
        point.tolerance = env->GetDoubleField(jPoint, toleranceField);
    }
    
    return point;
}

// Конвертация массива MotorCommand
std::vector<MotorCommand> convertMotorCommands(JNIEnv* env, jobjectArray commandsArray, jint count) {
    std::vector<MotorCommand> commands;
    if (!commandsArray || count <= 0) return commands;
    
    commands.reserve(count);
    for (jint i = 0; i < count; i++) {
        jobject jCmd = env->GetObjectArrayElement(commandsArray, i);
        if (jCmd) {
            commands.push_back(convertMotorCommand(env, jCmd));
            env->DeleteLocalRef(jCmd);
        }
    }
    
    return commands;
}

// Конвертация SpeedProfile (исправление бага #2 - используем vector)
SpeedProfileCore convertSpeedProfile(JNIEnv* env, jobject jProfile) {
    SpeedProfileCore profile{};
    
    if (!jProfile || !g_SpeedProfileClass) return profile;
    
    jfieldID portField = env->GetFieldID(g_SpeedProfileClass, "port", "B");
    jfieldID pointsField = env->GetFieldID(g_SpeedProfileClass, "points", 
        "[Lcom/example/lpstudio/PrinterController$SpeedProfilePoint;");
    jfieldID timeoutField = env->GetFieldID(g_SpeedProfileClass, "timeoutMs", "I");
    
    if (!portField || !pointsField || !timeoutField) return profile;
    
    profile.port = env->GetByteField(jProfile, portField);
    profile.timeoutMs = env->GetIntField(jProfile, timeoutField);
    
    jobjectArray pointsArray = (jobjectArray)env->GetObjectField(jProfile, pointsField);
    if (pointsArray) {
        jsize count = env->GetArrayLength(pointsArray);
        profile.points.reserve(count);
        
        for (jsize i = 0; i < count; i++) {
            jobject jPoint = env->GetObjectArrayElement(pointsArray, i);
            if (jPoint) {
                profile.points.push_back(convertSpeedProfilePoint(env, jPoint));
                env->DeleteLocalRef(jPoint);
            }
        }
        
        env->DeleteLocalRef(pointsArray);
    }
    
    return profile;
}

// Конвертация массива SpeedProfile
std::vector<SpeedProfileCore> convertSpeedProfilesToCore(JNIEnv* env, jobjectArray profilesArray, jint count) {
    std::vector<SpeedProfileCore> profiles;
    if (!profilesArray || count <= 0) return profiles;
    
    profiles.reserve(count);
    for (jint i = 0; i < count; i++) {
        jobject jProfile = env->GetObjectArrayElement(profilesArray, i);
        if (jProfile) {
            profiles.push_back(convertSpeedProfile(env, jProfile));
            env->DeleteLocalRef(jProfile);
        }
    }
    
    return profiles;
}
