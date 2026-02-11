cmake_minimum_required(VERSION 3.10)
project(SovereignThermalGovernor)

set(CMAKE_CXX_STANDARD 17)

include_directories(../include)

add_executable(nvme_governor 
    GovernorMain.cpp 
    ThermalGovernor.cpp
)

target_link_libraries(nvme_governor kernel32)
