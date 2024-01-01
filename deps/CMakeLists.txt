cmake_minimum_required(VERSION 3.20)
project(imgui LANGUAGES CXX)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB SOURCES  
                ${PROJECT_SOURCE_DIR}/imgui/imgui*.cpp
                ${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_sdl2.cpp
                ${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_sdlrenderer2.cpp)

add_library(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} SDL2::SDL2 ${OPENGL_LIBRARY})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/imgui ${PROJECT_SOURCE_DIR}/imgui/backends)