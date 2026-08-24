cmake_minimum_required(VERSION 4.0.3)
project(MotionCompiler LANGUAGES CXX)
set(CMAKE_CXX_STANDART 17)
set(CMAKE_CXX_STANDART_REQUIRED ON)

# Compile OpenCV
if(NOT TARGET opencv_world)
    message(STATUS "Building OpenCV...")
	
	# Create a separate OpenCV build
	set(OPENCV_BUILD_DIR "${CMAKE_BINARY_DIR}/opencv_build")
	set(OPENCV_INSTALL_DIR "${CMAKE_BINARY_DIR}/opencv_install")
	
	# OpenCV settings
	set(OPENCV_OPTIONS
	    -DBUILD_opencv_world=ON
		-DBUILD_EXAMPLES=OFF
		-DBUILD_TESTS=OFF
		-DBUILD_PERF_TESTS=OFF
		-DBUILD_opencv_python3=OFF
		-DWITH_IPP=OFF
		-DCMAKE_INSTALL_PREFIX=${OPENCV_INSTALL_DIR}
		-DCMAKE_BUILD_TYPE=Release
	)
	
	# Run the OpenCV configuration
	execute_process(
	    COMMAND ${CMAKE_COMMAND} -S "${CMAKE_SOURCE_DIR}/external/opencv" -B "${OPENCV_BUILD_DIR}" ${OPENCV_OPTIONS}
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		RESULT_VARIABLE OPENCV_CONFIG_RESULT
	)
	
	if(OPENCV_CONFIG_RESULT EQUAL 0)
	    # Compiling and installing OpenCV
		execute_process(
		    COMMAND ${CMAKE_COMMAND} --build "${OPENCV_BUILD_DIR}" --config Release --target install
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			RESULT_VARIABLE OPENCV_BUILD_RESULT
		)
		
		if(OPENCV_BUILD_RESULT EQUAL 0)
		    message(STATUS "OpenCV built successfully")
			
			set(OpenCV_DIR "${OPENCV_INSTALL_DIR}/x64/vc17/lib")
			
			# Adding a CMake search path
			list(APPEND CMAKE_PREFIX_PATH "${OpenCV_DIR}")
			list(APPEND CMAKE_PREFIX_PATH "${OPENCV_INSTALL_DIR}")
			
			# Looking for OpenCV
			find_package(OpenCV REQUIRED PATHS "${OpenCV_DIR}" NO_DEFAULT_PATH)
			
			if(OpenCV_FOUND)
			    message(STATUS "OpenCV found: ${OpenCV_DIR}")
				message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
			else()
			    message(FATAL_ERROR "OpenCV not found after building")
			endif()
		else()
		    message(FATAL_ERROR "Failed to build OpenCV")
		endif()
	else()
	    message(FATAL_ERROR "Failed to configure OpenCV")
	endif()
endif()

# Create shared library (DLL) instead of executable
add_library(MotionCompiler SHARED
	src/api/MotionCompilerAPI.h
	src/api/MotionCompilerAPI.cpp
	src/core/MotionCompiler.h
	src/core/MotionCompiler.cpp
	src/image/ImageProcessor.h
	src/image/ImageProcessor.cpp
	src/path/ContourOrganizer.h
	src/path/ContourOrganizer.cpp
	src/gcode/GCodeWriter.h
	src/gcode/GCodeWriter.cpp
	src/model/Point.h
	src/model/Contour.h
	src/model/ImageProfile.h
)

target_compile_definitions(MotionCompiler PRIVATE MOTIONCOMPILER_EXPORTS)

# For Windows DLL export symbols
if(WIN32)
    # Automatically export all symbols (simplified the process)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	
	# Optionally set output directories
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

if(OpenCV_FOUND)
    target_include_directories(MotionCompiler PRIVATE ${OpenCV_INCLUDE_DIRS})
	target_link_libraries(MotionCompiler PRIVATE ${OpenCV_LIBS})
	message(STATUS "OpenCV configured successfully")
	message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
	message(STATUS "OpenCV libs: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "OpenCV not found")
endif()
