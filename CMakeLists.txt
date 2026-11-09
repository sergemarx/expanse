cmake_minimum_required(VERSION 3.22)
project(Expanse VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(EXPANSE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND EXPANSE_FORMATS AU)
endif()

option(EXPANSE_COPY_AFTER_BUILD "Copy plugin to system folders after build" ON)

juce_add_plugin(Expanse
    COMPANY_NAME "YourName"            # <- change me
    PLUGIN_MANUFACTURER_CODE Ynam      # 4 chars, at least one uppercase
    PLUGIN_CODE Xpns                   # 4 chars, unique per plugin
    FORMATS ${EXPANSE_FORMATS}
    PRODUCT_NAME "Expanse"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${EXPANSE_COPY_AFTER_BUILD}

target_sources(Expanse PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/Util.h
    Source/DSP/Effects.h
    Source/DSP/FDN.h
    Source/DSP/Engine.h)

target_compile_definitions(Expanse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Expanse
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: offline DSP test (no JUCE needed at runtime)
add_executable(expanse_engine_test Tests/engine_test.cpp)
set_target_properties(expanse_engine_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
