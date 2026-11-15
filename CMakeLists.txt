cmake_minimum_required(VERSION 3.20)

project(BarnesHutNBodySimulator VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_executable(barnes_hut_simulator
    src/main.cpp
)
add_executable(nbody
    src/main.cpp
)

foreach(simulator_target IN ITEMS barnes_hut_simulator nbody)
    target_compile_features(${simulator_target} PRIVATE cxx_std_20)
    target_link_libraries(${simulator_target} PRIVATE Threads::Threads)
    set_target_properties(${simulator_target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(${simulator_target} PRIVATE
        $<$<CONFIG:Release>:-O3 -march=native -ffast-math>
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra>
    )
endforeach()

