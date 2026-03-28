cmake_minimum_required(VERSION 3.22)

project(OpenRiffBox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT for portable distribution
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE submodule
add_subdirectory(libs/JUCE)

# ASIO SDK: set JUCE_ASIO_HEADER_PATH if you have the ASIO SDK
# e.g. set(JUCE_ASIO_HEADER_PATH "${CMAKE_SOURCE_DIR}/libs/asio-sdk/common")

juce_add_plugin(OpenRiffBox
    COMPANY_NAME            "OpenRiffBox"
    PLUGIN_MANUFACTURER_CODE "Orfb"
    PLUGIN_CODE             "Orfb"
    FORMATS                 Standalone VST3
    PRODUCT_NAME            "OpenRiffBox"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Fx"
    ICON_BIG                "${CMAKE_SOURCE_DIR}/resources/icon_256.png"
    ICON_SMALL              "${CMAKE_SOURCE_DIR}/resources/icon_48.png"
)

target_sources(OpenRiffBox
    PRIVATE
        src/Main.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/dsp/EffectChain.cpp
        src/dsp/NoiseGate.cpp
        src/dsp/Distortion.cpp
        src/dsp/MetalDistortion.cpp
        src/dsp/DiodeDrive.cpp
        src/dsp/AmpSimSilver.cpp
        src/dsp/AmpSimGold.cpp
        src/dsp/AmpSimPlatinum.cpp
        src/dsp/TriodeStage.cpp
        src/dsp/PentodeStage.cpp
        src/dsp/AnalogDelay.cpp
        src/dsp/SpringReverb.cpp
        src/dsp/PlateReverb.cpp
        src/dsp/Chorus.cpp
        src/dsp/Flanger.cpp
        src/dsp/Phaser.cpp
        src/dsp/Vibrato.cpp
        src/dsp/Equalizer.cpp
        src/dsp/WaveshaperEngine.cpp
        src/dsp/TunerEngine.cpp
        src/ui/TunerPanel.cpp
        src/ui/TopBar.cpp
        src/ui/AmpSimSilverPanel.cpp
        src/ui/AmpSimGoldPanel.cpp
        src/ui/AmpSimPlatinumPanel.cpp
        src/ui/AmpSimSwitcherPanel.cpp
        src/ui/ChainListPanel.cpp
        src/ui/EffectDetailPanel.cpp
        src/ui/NoiseGatePanel.cpp
        src/ui/DistortionPanel.cpp
        src/ui/DiodeDrivePanel.cpp
        src/ui/DelayPanel.cpp
        src/ui/ReverbPanel.cpp
        src/ui/PlateReverbPanel.cpp
        src/ui/ReverbSwitcherPanel.cpp
        src/ui/ChorusPanel.cpp
        src/ui/FlangerPanel.cpp
        src/ui/PhaserPanel.cpp
        src/ui/VibratoPanel.cpp
        src/ui/ModulationSwitcherPanel.cpp
        src/ui/EQPanel.cpp
        src/ui/SidebarPanel.cpp
        src/ui/MainLayout.cpp
        src/ui/PresetBar.cpp
        src/ui/PresetBrowserPanel.cpp
        src/ui/AudioSettingsPanel.h
        src/ui/PlaceholderPanel.h
        src/ui/SettingsLookAndFeel.h
        src/preset/Preset.cpp
        src/preset/PresetManager.cpp
        src/tools/OfflineProcessor.cpp
)

target_include_directories(OpenRiffBox
    PRIVATE
        src
        src/dsp
        src/ui
        src/preset
        src/tools
)

juce_add_binary_data(OpenRiffBox_BinaryData
    SOURCES
        resources/fonts/Inter_18pt-Regular.ttf
        resources/fonts/Inter_18pt-SemiBold.ttf
        resources/fonts/Inter_18pt-Bold.ttf
        resources/fonts/MetalMania-Regular.ttf
        "resources/irs/Studio 57.wav"
        "resources/irs/In Your Face.wav"
        "resources/irs/Tight Box.wav"
        "resources/irs/Crystal 412.wav"
        "resources/irs/British Bite.wav"
        "resources/irs/Velvet Blues.wav"
        "resources/irs/Bass Colossus.wav"
        "resources/irs/Acoustic Body.wav"
        "resources/irs/Ambient Hall.wav"
        "resources/irs/Blackface Clean.wav"
        "resources/irs/Plexi Roar.wav"
        "resources/irs/Thunder Box.wav"
        "resources/irs/Tweed Gold.wav"
        "resources/irs/Vox Chime.wav"
)

target_compile_definitions(OpenRiffBox
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OpenRiffBox,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OpenRiffBox,JUCE_VERSION>"
        JUCE_ASIO=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    PRIVATE
        $<$<CONFIG:Debug>:JUCE_DEBUG=1>
)

# Post-build: assemble portable distribution folder
set(DIST_DIR "${CMAKE_BINARY_DIR}/dist/OpenRiffBox")
add_custom_command(TARGET OpenRiffBox_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/presets/factory"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/presets/user"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:OpenRiffBox_Standalone>"
        "${DIST_DIR}/OpenRiffBox.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/presets/factory"
        "${DIST_DIR}/presets/factory"
    COMMENT "Assembling portable distribution in ${DIST_DIR}"
)

target_link_libraries(OpenRiffBox
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        OpenRiffBox_BinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
