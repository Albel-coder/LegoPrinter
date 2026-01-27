cmake_minimum_required(VERSION 3.18.1)
project("printer-jni")

set(CMAKE_CXX_STANDARD 17)

# Флаги для совместимости с 16 KB page size (Android 15+)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=0x4000")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,max-page-size=0x4000")

# Список исходных файлов
add_library(printer-jni SHARED
    jni_globals.cpp      # Добавляем новый файл с глобальными переменными
    printer_runtime.cpp
    converter.cpp
    printer_jni.cpp
    interpreter_jni.cpp
    jni_safe_call.cpp
    jni_bridge.cpp
)

target_include_directories(printer-jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Установите свойство для выравнивания сегментов
set_target_properties(printer-jni PROPERTIES
    LINK_FLAGS "-Wl,-z,max-page-size=0x4000"
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

# Только необходимые флаги
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fexceptions")

# Отладочные флаги только для Debug
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()
