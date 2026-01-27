#include "printer_jni.h"
#include "jni_globals.h"
#include <vector>

// Вспомогательная функция для конвертации MotorCommand
MotorCommand convertMotorCommand(JNIEnv* env, jobject jCmd) {
    MotorCommand cmd{};
    
    jclass cls = env->GetObjectClass(jCmd);
    if (!cls) {
        JNI_LOGE("Failed to get MotorCommand class");
        return cmd;
    }
    
    jfieldID portField = env->GetFieldID(cls, "port", "B");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID revolutionsField = env->GetFieldID(cls, "revolutions", "D");
    
    if (portField && speedField && revolutionsField) {
        cmd.port = env->GetByteField(jCmd, portField);
        cmd.speed = env->GetByteField(jCmd, speedField);
        cmd.revolutions = env->GetDoubleField(jCmd, revolutionsField);
    } else {
        JNI_LOGE("Failed to get MotorCommand fields");
    }
    
    env->DeleteLocalRef(cls);
    return cmd;
}

// Вспомогательная функция для конвертации SpeedProfilePoint
SpeedProfilePoint convertSpeedProfilePoint(JNIEnv* env, jobject jPoint) {
    SpeedProfilePoint point{};
    
    if (!g_SpeedProfilePointClass) {
        JNI_LOGE("SpeedProfilePoint class not cached");
        return point;
    }
    
    jfieldID distanceField = env->GetFieldID(g_SpeedProfilePointClass, "distance", "D");
    jfieldID speedField = env->GetFieldID(g_SpeedProfilePointClass, "speed", "B");
    jfieldID toleranceField = env->GetFieldID(g_SpeedProfilePointClass, "tolerance", "D");
    
    if (distanceField && speedField && toleranceField) {
        point.distance = env->GetDoubleField(jPoint, distanceField);
        point.speed = env->GetByteField(jPoint, speedField);
        point.tolerance = env->GetDoubleField(jPoint, toleranceField);
    } else {
        JNI_LOGE("Failed to get SpeedProfilePoint fields");
    }
    
    return point;
}

// Конвертация массива MotorCommand
std::vector<MotorCommand> convertMotorCommands(JNIEnv* env, jobjectArray commandsArray, jint count) {
    std::vector<MotorCommand> commands;
    if (!commandsArray || count <= 0) {
        JNI_LOGE("Invalid commands array in convertMotorCommands");
        return commands;
    }
    
    commands.reserve(count);
    for (jint i = 0; i < count; i++) {
        jobject jCmd = env->GetObjectArrayElement(commandsArray, i);
        if (jCmd) {
            commands.push_back(convertMotorCommand(env, jCmd));
            env->DeleteLocalRef(jCmd);
        } else {
            JNI_LOGE("Null MotorCommand at index %d", i);
        }
    }
    
    JNI_LOGD("Converted %zu MotorCommands", commands.size());
    return commands;
}

// Конвертация SpeedProfile
SpeedProfileCore convertSpeedProfile(JNIEnv* env, jobject jProfile) {
    SpeedProfileCore profile{};
    
    if (!jProfile || !g_SpeedProfileClass) {
        JNI_LOGE("Invalid profile or class not cached in convertSpeedProfile");
        return profile;
    }
    
    jfieldID portField = env->GetFieldID(g_SpeedProfileClass, "port", "B");
    jfieldID pointsField = env->GetFieldID(g_SpeedProfileClass, "points", 
        "[Lcom/example/lpstudio/PrinterController$SpeedProfilePoint;");
    jfieldID timeoutField = env->GetFieldID(g_SpeedProfileClass, "timeoutMs", "I");
    
    if (!portField || !pointsField || !timeoutField) {
        JNI_LOGE("Failed to get SpeedProfile fields");
        return profile;
    }
    
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
            } else {
                JNI_LOGE("Null SpeedProfilePoint at index %zu", i);
            }
        }
        
        env->DeleteLocalRef(pointsArray);
    } else {
        JNI_LOGD("SpeedProfile has no points array");
    }
    
    JNI_LOGD("Converted SpeedProfile: port=%d, points=%zu, timeout=%d", 
            profile.port, profile.points.size(), profile.timeoutMs);
    return profile;
}

// Конвертация массива SpeedProfile
std::vector<SpeedProfileCore> convertSpeedProfilesToCore(JNIEnv* env, jobjectArray profilesArray, jint count) {
    std::vector<SpeedProfileCore> profiles;
    if (!profilesArray || count <= 0) {
        JNI_LOGE("Invalid profiles array in convertSpeedProfilesToCore");
        return profiles;
    }
    
    profiles.reserve(count);
    for (jint i = 0; i < count; i++) {
        jobject jProfile = env->GetObjectArrayElement(profilesArray, i);
        if (jProfile) {
            profiles.push_back(convertSpeedProfile(env, jProfile));
            env->DeleteLocalRef(jProfile);
        } else {
            JNI_LOGE("Null SpeedProfile at index %d", i);
        }
    }
    
    JNI_LOGD("Converted %zu SpeedProfiles", profiles.size());
    return profiles;
}
