cmake_minimum_required(VERSION 3.22)
project(SwoopFilter VERSION 0.1.0)

# Put JUCE next to this file:  git clone https://github.com/juce-framework/JUCE.git
add_subdirectory(JUCE)

juce_add_plugin(SwoopFilter
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Ynam
    PLUGIN_CODE Swpf
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Swoop Filter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE)

target_sources(SwoopFilter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SwoopFilter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SwoopFilter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
