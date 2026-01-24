cmake_minimum_required(VERSION 3.18.1)

project("printer-jni")

# Explicitly specifying static linking
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Flag for 16KB alignment
add_compile_options(-Wa,--noexecstack)
add_link_options(-Wl,--no-undefined)
add_link_options(-Wl,-z,noexecstack)
add_link_options(-Wl,-z,relro,-z,now)
add_link_options("-Wl,-z,max-page-size=16384")
add_link_options("-Wl,-z,common-page-size=16384")

# Path to header files
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Path to your .so files
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

# Import your library
add_library(printerdriver SHARED IMPORTED)
set_target_properties(printerdriver PROPERTIES
    IMPORTED_LOCATION ${LIB_DIR}/LegoPrinterCore.so
)

# Our JNI library
add_library(printer-jni SHARED
    printer_jni.cpp
)

# Linking libraries
target_link_libraries(printer-jni
    android
    log
    printerdriver
)

# Explicitly specify static linking of the C++ standard library
target_link_libraries(printer-jni
    -static-libstdc++
)