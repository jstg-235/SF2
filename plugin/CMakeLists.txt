include(FetchContent)

# Use a local JUCE checkout with -DSF2_JUCE_PATH=/path/to/JUCE, otherwise fetch the pinned release.
set(SF2_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")
set(SF2_JUCE_TAG "9.0.5" CACHE STRING "JUCE release tag to fetch")

if(SF2_JUCE_PATH)
    add_subdirectory(${SF2_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${SF2_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SF2_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SF2_FORMATS AU)
endif()
# AAX is deferred until there is an Avid developer account (docs/DESIGN.md).

juce_add_plugin(SF2
    COMPANY_NAME "SF2"
    PRODUCT_NAME "SF2"
    BUNDLE_ID "com.sf2.ambisonic"
    PLUGIN_MANUFACTURER_CODE Sftw
    PLUGIN_CODE Sf2a
    FORMATS ${SF2_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Spatial
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(SF2 PRIVATE
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Parameters.cpp)

target_compile_definitions(SF2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(TARGET sf2sofa)
    target_link_libraries(SF2 PRIVATE sf2sofa)
    target_compile_definitions(SF2 PRIVATE SF2_WITH_SOFA=1)
else()
    target_compile_definitions(SF2 PRIVATE SF2_WITH_SOFA=0)
endif()

target_link_libraries(SF2
    PRIVATE
        sf2dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tool: renders the window to a PNG (tools/Screenshot.cpp). Off by default.
option(SF2_BUILD_DEVTOOLS "Build developer tools" OFF)
if(SF2_BUILD_DEVTOOLS)
    juce_add_console_app(SF2Screenshot PRODUCT_NAME "SF2Screenshot")
    target_sources(SF2Screenshot PRIVATE ${CMAKE_SOURCE_DIR}/tools/Screenshot.cpp)
    target_compile_definitions(SF2Screenshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SF2Screenshot PRIVATE SF2 sf2dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
