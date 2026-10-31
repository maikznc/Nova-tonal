cmake_minimum_required (VERSION 3.22)

# Muss vor project() stehen
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimale macOS-Version")
set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal Binary (Apple Silicon + Intel)")

project (NOVA VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set_property (GLOBAL PROPERTY USE_FOLDERS YES)

#==============================================================================
# JUCE: entweder lokaler Checkout (-DMS_JUCE_PATH=...) oder automatischer Download
set (MS_JUCE_PATH "" CACHE PATH "Pfad zu einem lokalen JUCE-Checkout (leer = automatisch herunterladen)")

if (MS_JUCE_PATH)
    add_subdirectory ("${MS_JUCE_PATH}" JUCE EXCLUDE_FROM_ALL)
else()
    include (FetchContent)
    FetchContent_Declare (JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable (JUCE)
endif()

#==============================================================================
# Gemeinsamer Kern aller Plugins
juce_add_module (modules/ms_core)

# Gemeinsame Einstellungen für alle Plugin-Targets
set (MS_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1)

if (APPLE)
    set (MS_PLUGIN_FORMATS AU VST3 Standalone)
    set (MS_COPY_AFTER_BUILD TRUE)     # landet direkt in ~/Library/Audio/Plug-Ins
else()
    # Nur für Compile-Checks/Tests unter Linux: ohne ALSA/JACK und optionale X11-Extensions
    set (MS_PLUGIN_FORMATS VST3)
    set (MS_COPY_AFTER_BUILD FALSE)
    list (APPEND MS_COMMON_DEFINITIONS
        JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0)
endif()

add_subdirectory (plugins/TonalBalance)

option (MS_BUILD_TESTS "Analyse-Tests und UI-Snapshot bauen" OFF)

if (MS_BUILD_TESTS)
    add_subdirectory (tests)
endif()
