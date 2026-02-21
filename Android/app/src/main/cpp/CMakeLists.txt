cmake_minimum_required(VERSION 3.18.1)
project("printer-jni")

set(CMAKE_CXX_STANDARD 17)

# Flags for compatibility with 16 KB page size (Android 15+)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=0x4000")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=0x4000")

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

set(CORE_STATIC_LIB ${LIB_DIR}/LegoPrinterCore.a)

# List of source files
add_library(printer-jni SHARED
    converter.cpp
    printer_jni.cpp
	interpreter_jni.cpp
	include/TransportAndroid.cpp
	include/Interpreter.h
	include/ITransport.h
	include/LegoPrinterCore.h
	include/LegoDriverAPI.h
)

target_include_directories(printer-jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(printer-jni
	${CORE_STATIC_LIB}
	log
	android
)

set_target_properties(printer-jni PROPERTIES
    LINK_FLAGS "-Wl,-z,max-page-size=0x4000"
)

# Only necessary flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fexceptions")

# Debug flags for debugging only
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()
