cmake_minimum_required(VERSION 3.22)
project(ToboImager VERSION 0.1.0)

# the C runtime is linked in, so the plugin loads on a PC without the Visual C++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(JUCE)

juce_add_plugin(ToboImager
    COMPANY_NAME "Tobokegao"
    COMPANY_WEBSITE "https://github.com/tobokegao/ToboImager"
    PLUGIN_MANUFACTURER_CODE Tbkg
    PLUGIN_CODE Timg
    FORMATS VST3 Standalone
    PRODUCT_NAME "Tobo Imager"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Spatial Stereo
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(ToboImager PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(ToboImager PRIVATE cxx_std_17)

target_compile_definitions(ToboImager PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ToboImager
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# draws the panel to PNG files and works the mouse, without a host (test/panel_shot.cpp)
juce_add_console_app(PanelShot PRODUCT_NAME "PanelShot")
target_sources(PanelShot PRIVATE
    test/panel_shot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_compile_features(PanelShot PRIVATE cxx_std_17)
target_compile_definitions(PanelShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PanelShot PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# does on purpose what a user or a host should not do, and checks that nothing breaks (test/stress.cpp)
juce_add_console_app(StressTest PRODUCT_NAME "StressTest")
target_sources(StressTest PRIVATE
    test/stress.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_compile_features(StressTest PRIVATE cxx_std_17)
target_compile_definitions(StressTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(StressTest PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
