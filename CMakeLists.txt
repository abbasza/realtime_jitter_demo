cmake_minimum_required(VERSION 3.10)
project(rt_jitter_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-O2 -Wall -Wextra)

add_executable(baseline   src/baseline.c)
target_link_libraries(baseline   m)

add_executable(rt_version src/rt_version.c)
target_link_libraries(rt_version m pthread)
