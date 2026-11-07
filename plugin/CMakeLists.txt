cmake_minimum_required(VERSION 3.22)

option(MURMUR_BUILD_TESTS "Build the plain C++ engine test executable" OFF)

# Optional path to a JUCE checkout. If empty, a sibling ../juce or ./JUCE
# checkout is used, otherwise JUCE is fetched from GitHub.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE 8 source checkout (optional)")

if(APPLE)
    # Builds for this Mac's own architecture (arm64 on Apple Silicon).
    # For a universal build: cmake -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
endif()

project(Murmur VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
set(MURMUR_LOCAL_JUCE "")
if(JUCE_DIR)
    set(MURMUR_LOCAL_JUCE "${JUCE_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../juce/CMakeLists.txt")
    set(MURMUR_LOCAL_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/../juce")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    set(MURMUR_LOCAL_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

if(MURMUR_LOCAL_JUCE)
    message(STATUS "Murmur: using local JUCE at ${MURMUR_LOCAL_JUCE}")
    add_subdirectory("${MURMUR_LOCAL_JUCE}" juce)
else()
    message(STATUS "Murmur: no local JUCE found, fetching JUCE 8.0.15")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(APPLE)
    set(MURMUR_FORMATS AU VST3 Standalone)
    set(MURMUR_COPY TRUE)
else()
    set(MURMUR_FORMATS VST3 Standalone)
    # On Windows, copying into Program Files needs admin rights; the installer does it instead.
    set(MURMUR_COPY FALSE)
endif()

juce_add_plugin(Murmur
    COMPANY_NAME "Murmur Audio"
    BUNDLE_ID com.murmuraudio.murmur
    PLUGIN_MANUFACTURER_CODE Mrmu
    PLUGIN_CODE Mm2m
    FORMATS ${MURMUR_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${MURMUR_COPY}
    PRODUCT_NAME "Murmur"
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Murmur listens to your instrument or voice to convert it to MIDI."
)

# Sources include JUCE module headers directly, so juce_generate_juce_header()
# is intentionally not used.
target_sources(Murmur PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PitchEngine.cpp
)

target_compile_definitions(Murmur PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Murmur PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Optional engine tests (plain C++, no JUCE)
# ---------------------------------------------------------------------------
if(MURMUR_BUILD_TESTS)
    add_executable(engine_test tests/engine_test.cpp Source/PitchEngine.cpp)
    target_include_directories(engine_test PRIVATE Source)
    set_target_properties(engine_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endif()
