#pragma once

// Automatic platform and compiler detection
#if defined(_WIN32) || defined(_WIN64)
    #define MOTIONCOMPILER_WINDOWS 1
    #define MOTIONCOMPILER_ANDROID 0
#elif defined(__ANDROID__)
    #define MOTIONCOMPILER_WINDOWS 0
    #define MOTIONCOMPILER_ANDROID 1
#else
    #define MOTIONCOMPILER_WINDOWS 0
    #define MOTIONCOMPILER_ANDROID 0
#endif

// Export/Import settings
#if MOTIONCOMPILER_WINDOWS
    #ifdef MOTIONCOMPILER_EXPORTS
    #define MOTION_COMPILER_API __declspec(dllexport)
#else
    #define MOTION_COMPILER_API __declspec(dllimport)
#endif
#elif MOTIONCOMPILER_ANDROID
    // For Android with GCC/Clang
#if defined(__GNUC__) || defined(__clang__)
    #define MOTION_COMPILER_API __attribute__(visibility("default"))
#else
    #define MOTION_COMPILER_API
#endif
#else
    // For other platforms (Linus/macOS)
    #define MOTION_COMPILER_API
#endif

// C-style for maximum compatibility with C# and Java UI
#ifdef __cplusplus
extern "C" {
#endif

    typedef void* MotionCompilerHandle;

    MOTION_COMPILER_API MotionCompilerHandle CreateMotionCompiler();
    MOTION_COMPILER_API void DestroyMotionCompiler(MotionCompilerHandle compiler);

    MOTION_COMPILER_API bool CompileImageProfiles(MotionCompilerHandle handle, const char* inputFilename, const char* outputFilename);
    MOTION_COMPILER_API bool CompileCode(MotionCompilerHandle handle, const char* inputFilename, const char* outputFilename);

#ifdef __cplusplus
}
#endif
