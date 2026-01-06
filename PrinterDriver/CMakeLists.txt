cmake_minimum_required(VERSION 4.0.3 FATAL_ERROR)

project(LegoPrinter VERSION 1.0.0 DESCRIPTION "Lego Printer Native Library" LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_FOR_ANDROID "Build for Android as shared library" OFF)
option(BUILD_FOR_WINDOWS "Build for Windows as DLL" ON)
option(BUILD_WITH_SIMPLEBLE "Build with SimpleBLE library (Windows only)" ON)

# Automatic platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(BUILD_FOR_ANDROID ON CACHE BOOL "Build for Android" FORCE)
    set(BUILD_FOR_WINDOWS OFF CACHE BOOL "Build for Windows" FORCE)
    set(BUILD_WITH_SIMPLEBLE OFF CACHE BOOL "SimpleBLE not available for Android" FORCE)
elseif(WIN32)
    set(BUILD_FOR_WINDOWS ON CACHE BOOL "Build for Windows" FORCE)
    set(BUILD_FOR_ANDROID OFF CACHE BOOL "Build for Android" FORCE)
endif()

# Check for conflicts
if(BUILD_FOR_ANDROID AND BUILD_FOR_WINDOWS)
    message(FATAL_ERROR "Cannot build for both Android and Windows simultaneously")
endif()

if(NOT BUILD_FOR_ANDROID AND NOT BUILD_FOR_WINDOWS)
    message(FATAL_ERROR "Please select either BUILD_FOR_ANDROID or BUILD_FOR_WINDOWS")
endif()

# Show the selected configuration
message(STATUS "==========================================")
if(BUILD_FOR_ANDROID)
    message(STATUS "Building for ANDROID (Shared Library)")
    message(STATUS "Android ABI: ${ANDROID_ABI}")
    message(STATUS "Android Platform: ${ANDROID_PLATFORM}")
elseif(BUILD_FOR_WINDOWS)
    message(STATUS "Building for WINDOWS (DLL)")
    message(STATUS "Generator: ${CMAKE_GENERATOR}")
endif()
message(STATUS "==========================================")

# Compiler settings
if(MSVC)
    add_compile_options(/std:c++17 /Zc:__cplusplus /W3 /WX-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    # Runtime settings
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Settings for Android/Linux (Clang/GCC)
    add_compile_options(-Wall -Wextra -Wpedantic -fvisibility=hidden)
    if(BUILD_FOR_ANDROID)
        add_compile_options(-fPIC)
    endif()
endif()

# Windows DLL settings
if(BUILD_FOR_WINDOWS)
    add_definitions(-DUNICODE -D_UNICODE -DLEGOPRINTER_WINDOWS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Adding SimpleBLE for Windows only
if(BUILD_FOR_WINDOWS AND BUILD_WITH_SIMPLEBLE)
    message(STATUS "Adding SimpleBLE library")
    add_subdirectory(external/simpleble/simpleble)
endif()

# Main library
add_library(LegoPrinterCore SHARED
    src/LegoPrinterCore.cpp
    src/LegoPrinterCore.h
    src/IPrinter.h
)

# Include directories
target_include_directories(LegoPrinterCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Common/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Export definitions
if(BUILD_FOR_WINDOWS)
    target_compile_definitions(LegoPrinterCore 
        PRIVATE 
            LEGOPRINTERCORE_EXPORTS
        PUBLIC
            LEGOPRINTER_WINDOWS=1
    )
else()
    target_compile_definitions(LegoPrinterCore
        PUBLIC
            LEGOPRINTER_WINDOWS=0
    )
endif()

# Linking libraries
if(BUILD_FOR_WINDOWS AND BUILD_WITH_SIMPLEBLE)
    target_link_libraries(LegoPrinterCore
        PUBLIC
            simpleble::simpleble
    )
elseif(BUILD_FOR_ANDROID)
    # Android-specific libraries
    find_library(log-lib log)
    target_link_libraries(LegoPrinterCore
        PRIVATE
            ${log-lib}
    )
endif()

# Output file settings
if(BUILD_FOR_WINDOWS)
    set_target_properties(LegoPrinterCore PROPERTIES

OUTPUT_NAME "LegoPrinterCore"
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # For debugging
    set_target_properties(LegoPrinterCore PROPERTIES
        DEBUG_POSTFIX "_d"
        RELEASE_POSTFIX ""
    )
    
elseif(BUILD_FOR_ANDROID)
    set_target_properties(LegoPrinterCore PROPERTIES
        OUTPUT_NAME "LegoPrinterCore"
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs/${ANDROID_ABI}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs/${ANDROID_ABI}
    )
endif()

# Setting character visibility for Android
if(BUILD_FOR_ANDROID)
    set_target_properties(LegoPrinterCore PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# Optional: installation
install(TARGETS LegoPrinterCore
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Optional: Copy header files
install(FILES 
    src/LegoPrinterCore.h
    src/IPrinter.h
    DESTINATION include
)

message(STATUS "Build configuration completed successfully!")