# CMakeLists.txt
cmake_minimum_required(VERSION 3.18.1)

project("printer-jni")

# Удалите эту строку
# arguments.add("-DANDROID_STL=c++_shared")

# Добавьте явное указание статической линковки
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Флаг для 16KB выравнивания
add_compile_options(-Wa,--noexecstack)
add_link_options(-Wl,--no-undefined)
add_link_options(-Wl,-z,noexecstack)
add_link_options(-Wl,-z,relro,-z,now)
add_link_options("-Wl,-z,max-page-size=16384")
add_link_options("-Wl,-z,common-page-size=16384")

# Путь к заголовочным файлам
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Путь к вашим .so файлам
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

# Импортируем вашу библиотеку
add_library(printerdriver SHARED IMPORTED)
set_target_properties(printerdriver PROPERTIES
    IMPORTED_LOCATION ${LIB_DIR}/LegoPrinterCore.so
)

# Наша JNI библиотека
add_library(printer-jni SHARED
    printer_jni.cpp
)

# Связываем библиотеки
target_link_libraries(printer-jni
    android
    log
    printerdriver
)

# Явно указываем статическую линковку C++ стандартной библиотеки
target_link_libraries(printer-jni
    -static-libstdc++
)