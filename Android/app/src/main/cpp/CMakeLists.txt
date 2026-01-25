cmake_minimum_required(VERSION 3.18.1)
project("printer-jni")

set(CMAKE_CXX_STANDARD 17)

add_library(printer-jni SHARED
    printer_jni.cpp
    interpreter_jni.cpp
    jni_onLoad.cpp
)

target_include_directories(printer-jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

add_library(printerdriver SHARED IMPORTED)
set_target_properties(printerdriver PROPERTIES
    IMPORTED_LOCATION ${LIB_DIR}/LegoPrinterCore.so
)

add_library(interpreter SHARED IMPORTED)
set_target_properties(interpreter PROPERTIES
    IMPORTED_LOCATION ${LIB_DIR}/Interpreter.so
)

find_library(log-lib log)

target_link_libraries(printer-jni
    android
    ${log-lib}
    printerdriver
    interpreter
)

# Для отладки
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
