cmake_minimum_required(VERSION 3.28)

project(ghub_linux
    VERSION 0.0.1
    DESCRIPTION "Configurador de periféricos Logitech para CachyOS"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(GHUB_BUILD_APP "Compila o executável Qt Quick" ON)
option(GHUB_BUILD_TESTS "Compila os testes" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GhubWarnings)

add_subdirectory(src)

if(GHUB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
