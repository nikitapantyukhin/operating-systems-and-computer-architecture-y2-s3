cmake_minimum_required(VERSION 3.10)

project(lab2 C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(
    parent
    src/parent.c
    src/io_utils.c
)

add_executable(
    child
    src/child.c
    src/io_utils.c
)

target_compile_options(
    parent
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_compile_options(
    child
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
