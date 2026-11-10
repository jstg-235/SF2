add_library(sf2dsp STATIC
    src/AFormat.cpp
    src/Engine.cpp
    src/Fft.cpp
    src/MicProfiles.cpp
    src/Shotgun.cpp
    src/Speakers.cpp
)

target_include_directories(sf2dsp PUBLIC include)
target_compile_features(sf2dsp PUBLIC cxx_std_20)
set_target_properties(sf2dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(sf2dsp PRIVATE /W4)
else()
    target_compile_options(sf2dsp PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()
