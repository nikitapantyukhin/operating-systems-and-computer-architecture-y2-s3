cmake_minimum_required(VERSION 3.16)
project(lab3 C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(lab3 src/main.c)

target_link_libraries(lab3 PRIVATE Threads::Threads)
target_compile_options(lab3 PRIVATE -Wall -Wextra -O2)