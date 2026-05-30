cmake_minimum_required(VERSION 3.10)
project(QuantumCpp VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define directories
include_directories(include)

# Main executable
add_executable(QuantumCpp src/main.cpp)

# Optimization flags
if(MSVC)
    target_compile_options(QuantumCpp PRIVATE /W4 /O2)
else()
    target_compile_options(QuantumCpp PRIVATE -Wall -Wextra -O3)
endif()
