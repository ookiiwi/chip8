project(chip8_test LANGUAGES C CXX)

find_package(Catch2 3 REQUIRED)

add_executable(${PROJECT_NAME} chip8_test.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain chip8_core)
