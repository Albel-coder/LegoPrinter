#pragma once

#include "jni.h"
#include <vector>
#include "../include/LegoDriverAPI.h"

MotorCommand convertMotorCommand(JNIEnv* env, jobject jCmd);

std::vector<MotorCommand> convertMotorCommands(JNIEnv* env, jobjectArray commandsArray);

SpeedProfilePoint convertSpeedProfilePoint(JNIEnv* env, jobject jPoint);

SpeedProfile convertSpeedProfile(JNIEnv* env, jobject jProfile);

std::vector<SpeedProfile> convertSpeedProfiles(JNIEnv* env, jobjectArray profilesArray);