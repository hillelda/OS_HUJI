cmake_minimum_required(VERSION 3.16)
project(os_ex2)

set(CMAKE_CXX_STANDARD 14)

include_directories(resources)

add_executable(os_ex2
#        resources/demo_itimer.c
#        resources/demo_jmp.c
#        resources/demo_singInt_handler.c
        resources/test.cpp
        resources/Thread.h
        resources/uthreads.cpp
        resources/uthreads.h
        resources/utils.h
        )
