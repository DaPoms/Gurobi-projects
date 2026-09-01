cmake_minimum_required(VERSION 4.4.2)
project (MDMKP_Universal_Solver LANGUAGES CXX)

# These 3 steps were created with the help of gemini #############
# 1. Point CMake to your CPLEX installation
set(CPLEX_ROOT "C:/Program Files/IBM/ILOG/CPLEX_Studio_Community222" CACHE PATH "CPLEX Root Directory")

# 2. Add OR-Tools' internal CMake modules to CMAKE_MODULE_PATH so it can find FindCPLEX.cmake
list(APPEND CMAKE_MODULE_PATH "C:/OR_Tools_Versions/or-tools_source/cmake")
set(CPLEX_ROOT "C:/Program Files/IBM/ILOG/CPLEX_Studio_Community222" CACHE PATH "CPLEX Root Directory")
# 3. Find CPLEX first to define the CPLEX::CPLEX target
find_package(CPLEX REQUIRED)
###################################################################

find_package(ortools CONFIG REQUIRED)
add_executable(universal_MDMKP main.cpp)
target_link_libraries(universal_MDMKP PRIVATE ortools::ortools)

add_custom_command(TARGET universal_MDMKP POST_BUILD   #Also not made by me
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "C:/or-tools-installed/bin"
    $<TARGET_FILE_DIR:universal_MDMKP>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Program Files/IBM/ILOG/CPLEX_Studio_Community222/cplex/bin/x64_win64/cplex2220.dll"
    $<TARGET_FILE_DIR:universal_MDMKP>
)

# ONLY IMPORANT THINGS TO RUN
# cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/or-tools-installed"   RUN THIS ONCE ON FIRST TIME          
# cmake --build build --config Release   RUN THIS AFTERWARDS EACH TIME
# Above one forms into .exe to run
