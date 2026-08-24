#pragma once

#include <string>

class MotionCompiler {
public:
    MotionCompiler();
    ~MotionCompiler();

    bool compileImageProfiles(std::string inputFilename, std::string outputFilename, bool useSkeleton);

    bool compileCode(std::string inputFilename, std::string outputFilename);
};
