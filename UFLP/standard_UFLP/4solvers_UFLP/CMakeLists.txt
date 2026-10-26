cmake_minimum_required(VERSION 3.20)
project(MDMKP_Universal_Solver LANGUAGES CXX)

find_package(ortools CONFIG REQUIRED)

add_executable(CPSAT_UFLP 4solvers.cpp)
target_link_libraries(CPSAT_UFLP PRIVATE ortools::ortools)

# Optional: If OR-Tools exports any runtime DLLs you need copied over
add_custom_command(TARGET CPSAT_UFLP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "C:/or-tools-installed/bin"
    $<TARGET_FILE_DIR:CPSAT_UFLP>
)

# ONLY IMPORANT THINGS TO RUN
# cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/or-tools-installed"   RUN THIS ONCE ON FIRST TIME          
# cmake --build build --config Release   RUN THIS AFTERWARDS EACH TIME
# Above one forms into .exe to run


