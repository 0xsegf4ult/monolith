cmake_minimum_required(VERSION 3.30.5)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(monolith VERSION 0.1 LANGUAGES CXX C ASM)

add_subdirectory("kernel")
add_subdirectory("init")
