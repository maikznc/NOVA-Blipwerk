cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(NovaBlipwerk VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(NOVA_COPY_AFTER_BUILD "Copy the plug-ins into the user's plug-in folders after building" OFF)
option(NOVA_BUILD_TESTS "Build the headless test tool" OFF)

# ---- JUCE -----------------------------------------------------------------------
# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local copy instead of downloading.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---- Formats --------------------------------------------------------------------
set(NOVA_FORMATS VST3)
if(APPLE)
    list(APPEND NOVA_FORMATS AU Standalone)
elseif(WIN32)
    list(APPEND NOVA_FORMATS Standalone)
endif()

juce_add_binary_data(NovaAssets
    SOURCES
        Resources/Fonts/Michroma-Regular.ttf
        Resources/Fonts/BarlowSemiCondensed-SemiBold.ttf
        Resources/Fonts/IBMPlexMono-Medium.ttf)
set_target_properties(NovaAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(NovaBlipwerk
    COMPANY_NAME                "Nova"
    BUNDLE_ID                   com.nova.blipwerk
    PLUGIN_MANUFACTURER_CODE    Nova
    PLUGIN_CODE                 Nblw
    FORMATS                     ${NOVA_FORMATS}
    PRODUCT_NAME                "Nova Blipwerk"
    DESCRIPTION                 "Blip synth with a 16-step sequencer"
    VERSION                     ${PROJECT_VERSION}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${NOVA_COPY_AFTER_BUILD}
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Synth
    MICROPHONE_PERMISSION_ENABLED FALSE)

set(NOVA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthEngine.cpp
    Source/UI.cpp)

target_sources(NovaBlipwerk PRIVATE ${NOVA_SOURCES})

set(NOVA_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)
if(UNIX AND NOT APPLE)
    # headless Linux CI/test builds: no optional X11 extensions needed
    list(APPEND NOVA_DEFINES JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_compile_definitions(NovaBlipwerk PUBLIC ${NOVA_DEFINES})

target_link_libraries(NovaBlipwerk
    PRIVATE
        NovaAssets
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(APPLE OR WIN32)
    target_link_libraries(NovaBlipwerk PRIVATE juce::juce_audio_utils)
endif()

# ---- Headless tests (render, host sync, state, UI snapshot) ----------------------
if(NOVA_BUILD_TESTS)
    juce_add_console_app(NovaTests PRODUCT_NAME "NovaTests")
    target_sources(NovaTests PRIVATE Tests/NovaTests.cpp ${NOVA_SOURCES})
    target_compile_definitions(NovaTests PRIVATE ${NOVA_DEFINES} JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(NovaTests PRIVATE
        NovaAssets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)
endif()
