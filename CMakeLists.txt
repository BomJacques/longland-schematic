cmake_minimum_required(VERSION 3.24)

project(LonglandSchematic VERSION 0.3.0 LANGUAGES C CXX)

option(LONGLAND_BUILD_PLUGIN "Build the JUCE VST3 plug-in" ON)
option(LONGLAND_BUILD_TESTS "Build the framework-free DSP tests" ON)

add_library(LonglandDSP STATIC
    Source/LonglandDSP.cpp
    Source/LonglandDSP.h)
target_compile_features(LonglandDSP PUBLIC cxx_std_17)
target_include_directories(LonglandDSP PUBLIC Source)

if(MSVC)
    target_compile_options(LonglandDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(LonglandDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LONGLAND_BUILD_TESTS)
    enable_testing()
    add_executable(LonglandDSPTests Tests/DspTests.cpp)
    target_link_libraries(LonglandDSPTests PRIVATE LonglandDSP)
    add_test(NAME LonglandDSPTests COMMAND LonglandDSPTests)
endif()

if(LONGLAND_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(LonglandSchematic
        COMPANY_NAME "Longland"
        BUNDLE_ID "com.longland.schematic"
        PLUGIN_MANUFACTURER_CODE Lngl
        PLUGIN_CODE Lsch
        FORMATS VST3
        PRODUCT_NAME "Longland Schematic"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(LonglandSchematic PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/FactoryPresets.h)
    target_link_libraries(LonglandSchematic PRIVATE
        LonglandDSP
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
    target_compile_definitions(LonglandSchematic PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    juce_generate_juce_header(LonglandSchematic)
endif()
