cmake_minimum_required(VERSION 3.16)
project(os_ex4)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(yaara_test)

add_executable(os_ex4
        yaara_test/YaaraConstants.h
#        yaara_test/YaaraTest.cpp

        MemoryConstants.h
        PhysicalMemory.cpp
        PhysicalMemory.h
        SimpleTest.cpp

        VirtualMemory.cpp
        VirtualMemory.h)
