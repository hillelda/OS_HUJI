cmake_minimum_required(VERSION 3.16)
project(OS_ex3)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS -pthread)

include_directories(Resources)
include_directories(Resources/Barrier)

add_executable(OS_ex3
        #        Resources/Atomic/atomicdemo.cpp
        #        Resources/Atomic/atomicdemo_multivar.cpp
        #        Resources/Barrier/Barrier.cpp
        Resources/Barrier/Barrier.h
        #        Resources/Barrier/barrierdemo.cpp
        #        "Resources/Sample Client/SampleClient.cpp"
        Resources/MapReduceClient.h
        Resources/MapReduceFramework.cpp
        Resources/MapReduceFramework.h
#        tests/test1.cpp
                tests/test2.cpp
        #        tests/test3.cpp
        #        tests/test4.cpp
        )

#cmake_minimum_required(VERSION 3.21)
#project(OS_ex3)
#
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_FLAGS -pthread)
#
#include_directories(Resources)
#include_directories(Resources/Barrier)
#
#find_package(Threads REQUIRED)
#
#add_library(MapReduceFramework
#        Resources/MapReduceClient.h
#        Resources/MapReduceFramework.cpp
#        Resources/MapReduceFramework.h
#        Resources/Barrier/Barrier.cpp
#        Resources/Barrier/Barrier.h
#        # ------------- Add your own .h/.cpp files here -------------------
#        )
#
#
#set_property(TARGET MapReduceFramework PROPERTY CXX_STANDARD 11)
#target_compile_options(MapReduceFramework PUBLIC
#        -Wall
#        -Wextra
#        )
#target_include_directories(MapReduceFramework PUBLIC Resources)
## link pthreads to your framework
#target_link_libraries(MapReduceFramework PUBLIC Threads::Threads)
#
## Add tests
#add_subdirectory(mattanTests)