cmake_minimum_required(VERSION 3.10)
project(SAT_Solver)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -pedantic -pthread")

include_directories(include)
add_executable(sat_solver src/main.cpp src/DPLL.cpp)