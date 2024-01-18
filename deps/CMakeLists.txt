cmake_minimum_required(VERSION 3.20)
project(deps LANGUAGES CXX)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB SOURCES  
                ${PROJECT_SOURCE_DIR}/imgui/imgui*.cpp
                ${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_sdl2.cpp
                ${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_sdlrenderer2.cpp)

add_library(imgui ${SOURCES})
target_link_libraries(imgui SDL2::SDL2 ${OPENGL_LIBRARY})
target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR}/imgui ${PROJECT_SOURCE_DIR}/imgui/backends)

add_library(inih ${PROJECT_SOURCE_DIR}/inih/ini.c)
target_include_directories(inih PUBLIC ${PROJECT_SOURCE_DIR}/inih)