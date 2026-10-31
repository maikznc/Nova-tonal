juce_add_console_app (MSTests PRODUCT_NAME "MSTests")

target_sources (MSTests PRIVATE
    MSTests.cpp
    ../plugins/TonalBalance/Source/PluginProcessor.cpp
    ../plugins/TonalBalance/Source/PluginEditor.cpp
    ../plugins/TonalBalance/Source/BalanceCurveView.cpp
    ../plugins/TonalBalance/Source/RegionMetersView.cpp)

target_include_directories (MSTests PRIVATE ../plugins/TonalBalance/Source)

target_compile_definitions (MSTests PRIVATE
    ${MS_COMMON_DEFINITIONS}
    "JucePlugin_Name=\"NOVA Leitstern\"")

target_link_libraries (MSTests
    PRIVATE
        ms_core
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
