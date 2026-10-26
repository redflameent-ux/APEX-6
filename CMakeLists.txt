cmake_minimum_required(VERSION 3.22)
project(Apex6 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE: use a local copy if present, otherwise download it ----
set(JUCE_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Optional local JUCE checkout")
if(EXISTS "${JUCE_LOCAL_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_LOCAL_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

option(APEX_BUILD_TESTS "Build the offline render test" OFF)

set(APEX_SOURCES
    Source/Params.cpp
    Source/Presets.cpp
    Source/dsp/Modules.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(APEX_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_FLAC=1)

set(APEX_LIBS
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

juce_add_plugin(Apex6
    COMPANY_NAME "TRANCE-6"
    PLUGIN_MANUFACTURER_CODE Trc6
    PLUGIN_CODE Apx6
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "APEX-6 Guitar Tone Matrix"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Apex6 PRIVATE ${APEX_SOURCES})
target_compile_definitions(Apex6 PUBLIC ${APEX_DEFS})
target_link_libraries(Apex6
    PRIVATE ${APEX_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(APEX_BUILD_TESTS)
    juce_add_console_app(ApexRender PRODUCT_NAME "ApexRender")
    target_sources(ApexRender PRIVATE ${APEX_SOURCES} tools/RenderTest.cpp)
    target_compile_definitions(ApexRender PRIVATE ${APEX_DEFS} JUCE_STANDALONE_APPLICATION=1 APEX_RENDER_TEST=1)
    target_link_libraries(ApexRender PRIVATE ${APEX_LIBS} juce::juce_recommended_config_flags)
endif()
