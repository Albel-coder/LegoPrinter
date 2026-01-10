#include <jni.h>

static JavaVM* g_vm = nullptr;

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

JavaVM* simpleble_get_jvm() {
    return g_vm;
}