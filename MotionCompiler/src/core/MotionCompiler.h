#pragma once

#include <string>
#include "../api/MotionCompilerAPI.h"

class MotionCompiler {
public:
    explicit MotionCompiler();
    ~MotionCompiler();

    bool compileImageProfiles(std::string inputFilename, std::string outputFilename);

    bool compileCode(std::string inputFilename, std::string outputFilename);
};
