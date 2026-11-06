cmake_minimum_required (VERSION 3.22)
project (ZION_ARK VERSION 0.1.0)
set (CMAKE_CXX_STANDARD 17)

set (JUCE_DIR "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "JUCE checkout")
add_subdirectory (${JUCE_DIR} JUCE_build)

juce_add_plugin (ZionArk
    COMPANY_NAME "Siddha Quantum Nexus Music"
    COMPANY_WEBSITE "https://www.siddhaquantumnexus.com"
    BUNDLE_ID com.siddhaquantumnexus.zionark
    PLUGIN_MANUFACTURER_CODE SqnM
    PLUGIN_CODE ZnAk
    FORMATS AU VST3
    PRODUCT_NAME "Zion Ark"
    DESCRIPTION "Kingston dub studio: tape echo, spring, twin phase, big knob, siren, Ark tape"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Delay Reverb)

juce_add_binary_data (ZionArkUI SOURCES
    ui/index.html
    ${JUCE_DIR}/modules/juce_gui_extra/native/javascript/index.js
    ${JUCE_DIR}/modules/juce_gui_extra/native/javascript/check_native_interop.js)

target_sources (ZionArk PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions (ZionArk PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries (ZionArk
    PRIVATE ZionArkUI juce::juce_audio_utils juce::juce_gui_extra juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
