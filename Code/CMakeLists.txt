cmake_minimum_required(VERSION 3.20)
project(Code CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Code
    src/main.cpp
    src/bitstream.cpp
    src/cabac.cpp
    src/cabac_tables.cpp
    src/rans.cpp
)

target_include_directories(Code PRIVATE include)