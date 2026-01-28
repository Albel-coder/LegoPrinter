// jni_globals.h
#pragma once
#include <jni.h>

// Объявления глобальных переменных (без static!)
extern JavaVM* g_jvm;
extern jclass g_SpeedProfileClass;
extern jclass g_SpeedProfilePointClass;

// Функции для работы с глобальными переменными
bool isJNIInitialized();
JavaVM* GetJavaVM();
