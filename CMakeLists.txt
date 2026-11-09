cmake_minimum_required(VERSION 3.22)

project(SF2 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SF2_BUILD_TESTS  "Build the DSP unit tests" ON)
option(SF2_BUILD_PLUGIN "Build the JUCE plugin (fetches JUCE)" ON)

# Host-independent DSP: no JUCE dependency, unit-tested on its own.
add_subdirectory(dsp)

if(SF2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SF2_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
