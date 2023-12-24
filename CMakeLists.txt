cmake_minimum_required(VERSION 3.20)
project(chip8 LANGUAGES C VERSION 0.1)

option(BUILD_TESTING "Build tester (default: ON)" ON)

find_package(SDL2 REQUIRED)

add_library(chip8_core src/chip8.c)
target_include_directories(chip8_core PUBLIC include/)

add_executable(chip8 src/main.c)
target_link_libraries(chip8 PRIVATE SDL2::SDL2 chip8_core)

if (BUILD_TESTING)
    add_subdirectory(test)
endif()