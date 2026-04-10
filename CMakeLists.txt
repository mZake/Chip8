cmake_minimum_required(VERSION 3.15)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

project(Chip8 LANGUAGES CXX)

add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)

add_executable(Chip8 src/chip8.cpp)

target_compile_features(Chip8 PUBLIC cxx_std_17)
set_property(TARGET Chip8 PROPERTY CXX_EXTENSIONS OFF)

if(MSVC)
  target_compile_options(Chip8 PRIVATE /W4)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU|Clang")
  target_compile_options(Chip8 PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(Chip8 PUBLIC SDL3::SDL3)
