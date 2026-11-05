cmake_minimum_required(VERSION 3.22)

project(PedalSteel VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_SOURCE "C:/Users/Joshua/Documents/guitar_synth_midi/JUCE" CACHE PATH "JUCE source directory")
add_subdirectory("${JUCE_SOURCE}" "${CMAKE_BINARY_DIR}/JUCE")

set(PLUGIN_NAME "NVA Pedal Steel")
set(PLUGIN_CODE "Nvps")
set(PLUGIN_MANUFACTURER "Northern Valley Audio")
set(PLUGIN_MANUFACTURER_CODE "Nval")

juce_add_plugin(PedalSteel
    COMPANY_NAME "${PLUGIN_MANUFACTURER}"
    PLUGIN_MANUFACTURER_CODE "${PLUGIN_MANUFACTURER_CODE}"
    PLUGIN_CODE "${PLUGIN_CODE}"
    BUNDLE_ID "com.northernvalleyaudio.pedalsteel"
    FORMATS VST3 Standalone
    PRODUCT_NAME "${PLUGIN_NAME}"
    PLUGIN_NAME "${PLUGIN_NAME}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
)

juce_generate_juce_header(PedalSteel)

set(PEDALSTEEL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/dsp/PedalSteelEngine.cpp
)

target_sources(PedalSteel PRIVATE ${PEDALSTEEL_SOURCES})
target_include_directories(PedalSteel PRIVATE Source Source/dsp)

target_compile_definitions(PedalSteel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PedalSteel PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# UI Snapshot Tool: renders the plugin editor to a PNG for visual verification
# ---------------------------------------------------------------------------
juce_add_console_app(PedalSteelShot PRODUCT_NAME "PedalSteelShot")
juce_generate_juce_header(PedalSteelShot)
target_sources(PedalSteelShot PRIVATE
    tools/shot/ShotMain.cpp
    ${PEDALSTEEL_SOURCES}
)
target_include_directories(PedalSteelShot PRIVATE Source Source/dsp)
target_compile_definitions(PedalSteelShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JucePlugin_Name="NVA Pedal Steel"
    JucePlugin_Build_Standalone=0
)
target_link_libraries(PedalSteelShot PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Offline MIDI & Preset Renderer: renders MIDI transcriptions & presets to WAV
# for direct ear A/B verification against the 4 Splice reference WAVs
# ---------------------------------------------------------------------------
juce_add_console_app(PedalSteelRender PRODUCT_NAME "PedalSteelRender")
juce_generate_juce_header(PedalSteelRender)
target_sources(PedalSteelRender PRIVATE
    tools/render/RenderMain.cpp
    Source/Presets.cpp
    Source/dsp/PedalSteelEngine.cpp
)
target_include_directories(PedalSteelRender PRIVATE Source Source/dsp)
target_compile_definitions(PedalSteelRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(PedalSteelRender PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(PedalSteel PRIVATE /utf-8 /O2)
    target_compile_options(PedalSteelShot PRIVATE /utf-8 /O2)
    target_compile_options(PedalSteelRender PRIVATE /utf-8 /O2)
endif()
