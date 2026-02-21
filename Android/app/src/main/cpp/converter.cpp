#include "converter.h"
#include <android/log.h>

#define LOG_TAG "Converter"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

MotorCommand convertMotorCommand(JNIEnv* env, jobject jCmd) {
    MotorCommand cmd{};
    jclass cls = env->GetObjectClass(jCmd);
    if (!cls) {
        LOGE("convertMotorCommand: failed to get class");
        return cmd;
    }

    jfieldID portField = env->GetFieldID(cls, "port", "B");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID revField = env->GetFieldID(cls, "revolutions", "D");

    if (portField && speedField && revField) {
        cmd.port = env->GetByteField(jCmd, portField);
        cmd.speed = env->GetByteField(jCmd, speedField);
        cmd.revolutions = env->GetDoubleField(jCmd, revField);
    } else {
        LOGE("convertMotorCommand: field lookup failed");
    }

    env->DeleteLocalRef(cls);
    return cmd;
}

std::vector<MotorCommand> convertMotorCommands(JNIEnv* env, jobjectArray commandsArray) {
    std::vector<MotorCommand> commands;
    if (!commandsArray) return commands;

    jsize count = env->GetArrayLength(commandsArray);
    commands.reserve(count);

    for (jsize i = 0; i < count; ++i) {
        jobject jCmd = env->GetObjectArrayElement(commandsArray, i);
        if (jCmd) {
            commands.push_back(convertMotorCommand(env, jCmd));
            env->DeleteLocalRef(jCmd);
        }
    }
    return commands;
}

SpeedProfilePoint convertSpeedProfilePoint(JNIEnv* env, jobject jPoint) {
    SpeedProfilePoint point{};
    jclass cls = env->GetObjectClass(jPoint);
    if (!cls) {
        LOGE("convertSpeedProfilePoint: failed to get class");
        return point;
    }

    jfieldID distField = env->GetFieldID(cls, "distance", "D");
    jfieldID speedField = env->GetFieldID(cls, "speed", "B");
    jfieldID tolField = env->GetFieldID(cls, "tolerance", "D");

    if (distField && speedField && tolField) {
        point.distance = env->GetDoubleField(jPoint, distField);
        point.speed = env->GetByteField(jPoint, speedField);
        point.tolerance = env->GetDoubleField(jPoint, tolField);
    } else {
        LOGE("convertSpeedProfilePoint: field lookup failed");
    }

    env->DeleteLocalRef(cls);
    return point;
}

SpeedProfile convertSpeedProfile(JNIEnv* env, jobject jProfile) {
    SpeedProfile profile{};
	profile.points = nullptr;
	
    jclass cls = env->GetObjectClass(jProfile);
    if (!cls) {
        LOGE("convertSpeedProfile: failed to get class");
        return profile;
    }

    jfieldID portField = env->GetFieldID(cls, "port", "B");
    jfieldID pointsField = env->GetFieldID(cls, "points",
        "[Lcom/example/lpstudio/PrinterController$SpeedProfilePoint;");
    jfieldID timeoutField = env->GetFieldID(cls, "timeoutMs", "I");

    if (portField && pointsField && timeoutField) {
        profile.port = env->GetByteField(jProfile, portField);
        profile.timeoutMs = env->GetIntField(jProfile, timeoutField);

        jobjectArray pointsArray = (jobjectArray)env->GetObjectField(jProfile, pointsField);
        if (pointsArray) {
            jsize count = env->GetArrayLength(pointsArray);
            profile.count = static_cast<int>(count);
            if (count > 0) {
                profile.points = new SpeedProfilePoint[count];
                for (jsize i = 0; i < count; ++i) {
                    jobject jPoint = env->GetObjectArrayElement(pointsArray, i);
                    if (jPoint) {
                        profile.points[i] = convertSpeedProfilePoint(env, jPoint);
                        env->DeleteLocalRef(jPoint);
                    }
                }
            } else {
                profile.points = nullptr;
            }
            env->DeleteLocalRef(pointsArray);
        }
    } else {
        LOGE("convertSpeedProfile: field lookup failed");
    }

    env->DeleteLocalRef(cls);
    return profile;
}

std::vector<SpeedProfile> convertSpeedProfiles(JNIEnv* env, jobjectArray profilesArray) {
    std::vector<SpeedProfile> profiles;
    if (!profilesArray) return profiles;

    jsize count = env->GetArrayLength(profilesArray);
    profiles.reserve(count);

    for (jsize i = 0; i < count; ++i) {
        jobject jProfile = env->GetObjectArrayElement(profilesArray, i);
        if (jProfile) {
            profiles.push_back(convertSpeedProfile(env, jProfile));
            env->DeleteLocalRef(jProfile);
        }
    }
    return profiles;
}
