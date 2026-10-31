juce_add_plugin (TonalBalance
    PRODUCT_NAME                "NOVA Leitstern"
    COMPANY_NAME                "NOVA"
    BUNDLE_ID                   "com.nova.leitstern"
    PLUGIN_MANUFACTURER_CODE    Nova
    PLUGIN_CODE                 Lstn
    FORMATS                     ${MS_PLUGIN_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Analyzer
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${MS_COPY_AFTER_BUILD})

target_sources (TonalBalance PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BalanceCurveView.cpp
    Source/RegionMetersView.cpp)

target_compile_definitions (TonalBalance PUBLIC ${MS_COMMON_DEFINITIONS})

target_link_libraries (TonalBalance
    PRIVATE
        ms_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
