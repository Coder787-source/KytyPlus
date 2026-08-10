cmake_minimum_required(VERSION 3.10)
project(KytyPS5Tests)

set(CMAKE_CXX_STANDARD 17)

# Include common source directory
include_directories(C:/Users/Keshav/Desktop/Ps5Emu/KytyPS5/src/common)

# Add the hardware accuracy test
add_executable(hardware_accuracy_test hardware_accuracy_test.cpp)

# Link against common library if applicable, or just compile as standalone for the test
# Since we used headers, we just need the source files if we have .cpp implementations
# For this specific test, we only used headers, so it's a standalone translation unit.
