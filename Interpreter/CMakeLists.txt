cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(GcodeInterpreter VERSION 1.0.0 DESCRIPTION "G-code Interpreter Library" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Defining the architecture for folder naming
if(NOT DEFINED ARCH_NAME)
    if(ANDROID)
        # For Android, we use ABI as the architecture name
        set(ARCH_NAME ${ANDROID_ABI})
    elseif(WIN32)
        # For Windows, we determine the architecture
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(ARCH_NAME "win64")
        else()
            set(ARCH_NAME "win32")
        endif()
    else()
        set(ARCH_NAME "unknown")
    endif()
endif()

# Platform definition
if(ANDROID)
    # Settings for Android
    set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK_HOME})
    set(CMAKE_ANDROID_STL_TYPE c++_static)
    set(CMAKE_ANDROID_ARM_NEON ON)
    
    # For Android, we always use UNICODE
    add_definitions(-DUNICODE -D_UNICODE -DANDROID)
    
    # Hide symbols by default
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
    
    # Compiler options for Android
    add_compile_options(-fvisibility=hidden)
    
elseif(WIN32)
    # Original settings for Windows
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Compiler options
if(MSVC AND NOT ANDROID)
    add_compile_options(/std:c++17)
    add_compile_options(/Zc:__cplusplus)
endif()

# Creating a library
add_library(GcodeInterpreter SHARED
    src/Interpreter.cpp
    src/Interpreter.h
    src/IPrinter.h
)

# Include directories
target_include_directories(GcodeInterpreter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Setting up output directories INSIDE the project root directory
# Create a convenient structure: <project root>/build/<platform>/<architecture>/<configuration>/
set(OUTPUT_ROOT_DIR "${CMAKE_SOURCE_DIR}/build")

if(ANDROID)
    # For Android: build/android/<abi>/<config>/
    set(BASE_OUTPUT_DIR "${OUTPUT_ROOT_DIR}/android/${ANDROID_ABI}")
    
    # Remove the "lib" prefix for Android (optional, but usually required)
    set_target_properties(GcodeInterpreter PROPERTIES
        PREFIX ""
        OUTPUT_NAME "GcodeInterpreter"
        LIBRARY_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/$<CONFIG>"
    )
    
    # Add the necessary libraries for Android
    target_link_libraries(GcodeInterpreter
        PUBLIC
            log      # Logging Android
            android  # Native API Android
    )
    
elseif(WIN32)
    # For Windows: build/windows/<arch>/<config>/
    set(BASE_OUTPUT_DIR "${OUTPUT_ROOT_DIR}/windows/${ARCH_NAME}")
    
    set_target_properties(GcodeInterpreter PROPERTIES
        OUTPUT_NAME "GcodeInterpreter"
        LIBRARY_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/$<CONFIG>"
    )
    
    target_compile_definitions(GcodeInterpreter 
        PRIVATE 
            GCODEINTERPRETER_EXPORTS
    )
endif()