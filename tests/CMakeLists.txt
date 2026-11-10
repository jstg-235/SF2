include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(sf2dsp_tests
    AFormatTests.cpp
    EngineTests.cpp
    GeometryTests.cpp
    ShotgunTests.cpp
)
target_link_libraries(sf2dsp_tests PRIVATE sf2dsp GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(sf2dsp_tests)
