cmake_minimum_required(VERSION 4.3)
project(superfetchlib)

set(CMAKE_CXX_STANDARD 20)

add_executable(superfetchlib main.cpp
        include/superfetch/superfetch.h
        include/superfetch/nt.h)

target_include_directories(superfetchlib PRIVATE include)
target_link_libraries(superfetchlib PRIVATE ntdll.lib)