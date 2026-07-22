CMAKE_MINIMUM_REQUIRED(VERSION 3.30.5)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(monolith VERSION 0.1 LANGUAGES C ASM)

add_subdirectory("kernel")
add_subdirectory("userspace")
