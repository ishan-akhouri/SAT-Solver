cmake_minimum_required(VERSION 3.10)
project(SAT_Solver)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -pedantic -pthread")
include_directories(include)
# Common source files
set(COMMON_SOURCES
    src/DPLL.cpp
    src/CDCL.cpp
    src/ClauseDatabase.cpp
    src/CDCLSolverIncremental.cpp
    src/ClauseMinimizer.cpp
    src/PortfolioManager.cpp 
    src/Preprocessor.cpp
)


add_executable(sat_solver src/main.cpp ${COMMON_SOURCES})

add_executable(sat_solver_incremental src/main_incremental.cpp ${COMMON_SOURCES})

add_executable(sat_solver_portfolio src/main_portfolio.cpp ${COMMON_SOURCES})

add_executable(sat_solver_preprocessor src/main_preprocessor.cpp ${COMMON_SOURCES})