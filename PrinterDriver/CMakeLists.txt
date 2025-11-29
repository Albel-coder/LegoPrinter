cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(LegoPrinter VERSION 1.0.0 DESCRIPTION "Lego Printer Native Library" LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/std:c++17)
    add_compile_options(/Zc:__cplusplus)
endif()

# Windows DLL Settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Adding SimpleBLE as a subproject
add_subdirectory(external/simpleble/simpleble)

# Creating a DLL library
add_library(LegoPrinterCore SHARED
    src/LegoPrinterCore.cpp
    src/LegoPrinterCore.h
)

# Specify header files
target_include_directories(LegoPrinterCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src  # To include headers in the src folder
		${CMAKE_CURRENT_SOURCE_DIR}/../Common/include # path to shared files
)

# Linking dependencies
target_link_libraries(LegoPrinterCore
    PUBLIC
        simpleble::simpleble  # Suggested target name from SimpleBLE
)

# Setting up output directories
set_target_properties(LegoPrinterCore PROPERTIES
    OUTPUT_NAME "LegoPrinterCore"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin  # For DLLs on Windows
)

# Setting up symbol export for Windows
if(WIN32)
    target_compile_definitions(LegoPrinterCore 
        PRIVATE 
            LEGOPRINTERCORE_EXPORTS
    )
endif()