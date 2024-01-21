cmake_minimum_required(VERSION 3.18)
project(chip8 LANGUAGES C CXX VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tester (default: ON)" ON)

find_package(SDL2 REQUIRED)
add_subdirectory(deps)

add_library(chip8_core src/chip8.c)
target_include_directories(chip8_core PUBLIC include/)

add_executable(chip8 src/main.cc src/c8_profiler.cc src/config.c src/loader.cc src/audio.cc)
target_link_libraries(chip8 PRIVATE SDL2::SDL2 chip8_core imgui inih)
