cmake_minimum_required(VERSION 3.18.1)

project("printer-jni")

# Настройки компиляции
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# Путь к заголовочным файлам
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Путь к вашим .so файлам
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

# Импортируем вашу библиотеку
add_library(printerdriver SHARED IMPORTED)
set_target_properties(printerdriver PROPERTIES
    IMPORTED_LOCATION ${LIB_DIR}/libprinterdriver.so
)

# Наша JNI библиотека (proxy)
add_library(printer-jni SHARED
    printer_jni.cpp
)

# Связываем библиотеки
target_link_libraries(printer-jni
    android
    log
    printerdriver
)

# Установка свойств для релиза и отладки
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -DDEBUG")
