cmake_minimum_required(VERSION 3.25)

project(coherence)

# Fetch argparse
include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v2.9
)
FetchContent_MakeAvailable(argparse)

option(DEBUG "Enable debug mode" OFF)
option(USE_WRITE_BUFFER "Enable write buffer" OFF)

include_directories(include)

add_library(trace STATIC src/trace.cpp)
target_compile_features(trace PRIVATE cxx_std_20)
target_compile_options(trace PRIVATE -Wall -Wpedantic -O3)

add_library(mesi STATIC src/protocols/mesi.cpp)
target_compile_features(mesi PRIVATE cxx_std_20)
target_compile_options(mesi PRIVATE -Wall -Wpedantic -O3)

if(DEBUG)
    target_compile_definitions(mesi PRIVATE -DDEBUG_FLAG)
endif()

add_executable(coherence
    src/main.cpp
    src/statistics.cpp
    src/parser.cpp
    src/cache.cpp
    src/bus.cpp
    src/memory_controller.cpp
    src/write_buffer.cpp
)
target_link_libraries(coherence PRIVATE argparse trace mesi)
target_compile_features(coherence PRIVATE cxx_std_20)
target_compile_options(coherence PRIVATE -Wall -Wpedantic -O3)

if(DEBUG)
    target_compile_definitions(coherence PRIVATE -DDEBUG_FLAG)
endif()

if(USE_WRITE_BUFFER)
    target_compile_definitions(coherence PRIVATE -DUSE_WRITE_BUFFER)
endif()