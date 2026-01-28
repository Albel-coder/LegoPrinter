// jni_globals.cpp
#include "jni_globals.h"

// Единственные определения глобальных переменных
JavaVM* g_jvm = nullptr;
jclass g_SpeedProfileClass = nullptr;
jclass g_SpeedProfilePointClass = nullptr;

// Реализация функций
bool isJNIInitialized() {
    return g_jvm != nullptr;
}

JavaVM* GetJavaVM() {
    return g_jvm;
}
