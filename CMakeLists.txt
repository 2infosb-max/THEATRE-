cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(Theatre VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(THEATRE_FORMATS VST3 AU)
else()
    set(THEATRE_FORMATS VST3)
endif()

juce_add_plugin(Theatre
    COMPANY_NAME "Sound Brewery"
    COMPANY_WEBSITE "https://cinebrewery.in"
    BUNDLE_ID "com.soundbrewery.theatre"
    PLUGIN_MANUFACTURER_CODE SBrw
    PLUGIN_CODE Thtr
    PRODUCT_NAME "Theatre"
    DESCRIPTION "Cinema sound coverage visualiser"
    FORMATS ${THEATRE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Analyzer Surround
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Theatre PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Theatre PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Theatre
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless test harness (not part of the plugin): cmake -DTHEATRE_TESTS=ON
option(THEATRE_TESTS "Build the headless test harness" OFF)
if(THEATRE_TESTS)
    juce_add_console_app(TheatreTest PRODUCT_NAME "TheatreTest")
    target_sources(TheatreTest PRIVATE Tests/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(TheatreTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Theatre" JucePlugin_Build_Standalone=0)
    target_link_libraries(TheatreTest PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
