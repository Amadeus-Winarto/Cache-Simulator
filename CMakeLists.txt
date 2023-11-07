cmake_minimum_required(VERSION 3.25)

project(coherence)

# Fetch argparse
include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

include_directories(include)

add_library(trace STATIC src/trace.cpp)
target_compile_features(trace PRIVATE cxx_std_20)

add_library(mesi STATIC src/protocols/mesi.cpp)
target_compile_features(mesi PRIVATE cxx_std_20)
target_compile_definitions(mesi PRIVATE DEBUG_FLAG)

add_executable(coherence
    src/main.cpp
    src/parser.cpp
    src/cache.cpp
    src/bus.cpp
    src/memory_controller.cpp
)
target_link_libraries(coherence PRIVATE argparse trace mesi)
target_compile_features(coherence PRIVATE cxx_std_20)

target_compile_options(coherence PRIVATE -Wall -Wpedantic)
target_compile_options(coherence PRIVATE -fsanitize=address)
target_link_options(coherence PRIVATE -fsanitize=address)