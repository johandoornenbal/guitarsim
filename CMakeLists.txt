cmake_minimum_required(VERSION 3.22)
project(GuitarSim VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
add_library(guitar_engine Source/ChimeyGuitarEngine.cpp Source/ChimeyGuitar2Engine.cpp Source/ChimeyDeluxeEngine.cpp)
set_target_properties(guitar_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(guitar_engine PUBLIC Source)
if(MSVC)
  target_compile_definitions(guitar_engine PUBLIC _USE_MATH_DEFINES)
endif()
if(BUILD_TESTING)
  add_executable(dsp_tests tests/DspTests.cpp tests/ReferenceChimeyGuitar.cpp tests/ReferenceChimeyGuitar2.cpp tests/ReferenceChimeyDeluxe.cpp)
  target_include_directories(dsp_tests PRIVATE tests/shim)
  target_link_libraries(dsp_tests PRIVATE guitar_engine)
  add_test(NAME dsp_and_upstream_reference COMMAND dsp_tests)
endif()
option(GUITARSIM_DSP_ONLY "Build DSP and reference tests without JUCE" OFF)
if(GUITARSIM_DSP_ONLY)
  return()
endif()
include(FetchContent)
set(JUCE_PATH "" CACHE PATH "Optional existing JUCE 8.0.12 checkout")
if(JUCE_PATH)
  add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
  FetchContent_Declare(JUCE
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/29396c22c93392d6738e021b83196283d6e4d850)
  FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(GuitarSim
  COMPANY_NAME "Doornenbal Audio"
  BUNDLE_ID "nl.doornenbal.guitarsim"
  PLUGIN_MANUFACTURER_CODE Jdau
  PLUGIN_CODE Gtsm
  FORMATS VST3
  PRODUCT_NAME "GuitarSim"
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Fx Distortion)
juce_generate_juce_header(GuitarSim)
target_sources(GuitarSim PRIVATE Source/Plugin.cpp)
target_compile_definitions(GuitarSim PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(GuitarSim PRIVATE guitar_engine juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(BUILD_TESTING)
  add_executable(plugin_checks tests/PluginChecks.cpp)
  target_include_directories(plugin_checks PRIVATE "${JUCE_MODULES_DIR}")
  target_compile_definitions(plugin_checks PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
  target_link_libraries(plugin_checks PRIVATE GuitarSim)
  add_test(NAME plugin_wrapper COMMAND plugin_checks)
endif()
