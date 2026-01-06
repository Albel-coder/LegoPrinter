#pragma once

#ifdef GCODEGENERATOR_EXPORTS
#define GCODE_GENERATOR_API __declspec(dllexport)
#else
#define GCODE_GENERATOR_API __declspec(dllimport)
#endif

#include <cstdint>

typedef void* GcodeGeneratorHandle;

extern "C"
{
	GCODE_GENERATOR_API GcodeGeneratorHandle CreateGenerator();
	GCODE_GENERATOR_API void DestroyGenerator(GcodeGeneratorHandle handle);

	GCODE_GENERATOR_API bool GenerateImageProfiles(GcodeGeneratorHandle handle, const char* inputFilename, const char* outputFilename);
	GCODE_GENERATOR_API bool GenerateCode(GcodeGeneratorHandle handle, const char* inputFilename, const char* outputFilename);
}