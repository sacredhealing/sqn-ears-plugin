cmake_minimum_required (VERSION 3.22)
project (SQN_EARS VERSION 2.0.0)
set (CMAKE_CXX_STANDARD 17)

set (JUCE_DIR "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "JUCE checkout")
add_subdirectory (${JUCE_DIR} JUCE_build)

juce_add_plugin (SqnEars
    COMPANY_NAME "Siddha Quantum Nexus Music"
    COMPANY_WEBSITE "https://www.siddhaquantumnexus.com"
    BUNDLE_ID com.siddhaquantumnexus.sqnears
    PLUGIN_MANUFACTURER_CODE SqnM
    PLUGIN_CODE SqEr
    FORMATS AU VST3
    PRODUCT_NAME "SQN Ears"
    DESCRIPTION "Listens to the master and fixes it"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Mastering)

juce_add_binary_data (SqnEarsUI SOURCES
    ui/index.html
    ${JUCE_DIR}/modules/juce_gui_extra/native/javascript/index.js
    ${JUCE_DIR}/modules/juce_gui_extra/native/javascript/check_native_interop.js)

target_sources (SqnEars PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions (SqnEars PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries (SqnEars
    PRIVATE SqnEarsUI juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
