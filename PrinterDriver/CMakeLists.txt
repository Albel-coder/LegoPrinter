cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(LegoPrinter VERSION 1.0.0 DESCRIPTION "Lego Printer Native Library" LANGUAGES CXX)

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/std:c++17)
    add_compile_options(/Zc:__cplusplus)
endif()

# Настройки для Windows DLL
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Добавляем SimpleBLE как подпроект
add_subdirectory(external/simpleble/simpleble)

# Создаем DLL библиотеку
add_library(LegoPrinterCore SHARED
    src/LegoPrinterCore.cpp
    src/LegoPrinterCore.h
    # Добавьте здесь другие исходные файлы, если они есть
)

# Указываем заголовочные файлы
target_include_directories(LegoPrinterCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src  # Для включения заголовков в папке src
		${CMAKE_CURRENT_SOURCE_DIR}/../Common/include # путь к общим файлам
)

# Линкуем зависимости
target_link_libraries(LegoPrinterCore
    PUBLIC
        simpleble::simpleble  # Предполагаемое имя target из SimpleBLE
)

# Настраиваем выходные директории
set_target_properties(LegoPrinterCore PROPERTIES
    OUTPUT_NAME "LegoPrinterCore"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin  # Для DLL на Windows
)

# Настройка экспорта символов для Windows
if(WIN32)
    target_compile_definitions(LegoPrinterCore 
        PRIVATE 
            LEGOPRINTERCORE_EXPORTS
    )
endif()