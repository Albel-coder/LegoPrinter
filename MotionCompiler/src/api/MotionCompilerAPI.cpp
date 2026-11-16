#include "MotionCompilerAPI.h"
#include "../core/MotionCompiler.h"

#ifdef __cplusplus
extern "C" {
#endif

    MOTION_COMPILER_API MotionCompilerHandle CreateMotionCompiler() {
        return new MotionCompiler();
    }

    MOTION_COMPILER_API void DestroyMotionCompiler(MotionCompilerHandle compiler) {
        delete static_cast<MotionCompiler*>(compiler);
    }

    MOTION_COMPILER_API bool CompileImageProfiles(MotionCompilerHandle handle,
        const char* inputFilename,
        const char* outputFilename) {
        if (!handle || !inputFilename || !outputFilename) return false;
        auto* compiler = static_cast<MotionCompiler*>(handle);
        return compiler->compileImageProfiles(inputFilename, outputFilename);
    }

    MOTION_COMPILER_API bool CompileCode(MotionCompilerHandle handle,
        const char* inputFilename,
        const char* outputFilename) {
        if (!handle || !inputFilename || !outputFilename) return false;
        auto* compiler = static_cast<MotionCompiler*>(handle);
        return compiler->compileCode(inputFilename, outputFilename);
    }

#ifdef __cplusplus
} // extern "C"
#endif
