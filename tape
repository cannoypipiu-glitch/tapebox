build/
.vs/
.idea/
*.user

cmake_minimum_required(VERSION 3.22)

# Universal binary no macOS e runtime estatico no Windows (sem precisar de redistributable)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(TapeBox VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TapeBox
  COMPANY_NAME "TapeBox Audio"
  PLUGIN_MANUFACTURER_CODE Tbxa
  PLUGIN_CODE Tbx1
  FORMATS VST3
  PRODUCT_NAME "TapeBox"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TapeBox PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h)

target_compile_definitions(TapeBox PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TapeBox
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags)

# TapeBox

Plugin VST3 de saturação de fita e lo-fi (JUCE + CMake).

**Controles:** Drive, Color, Wow/Flut, Noise, Tone, Output + chaves Age (New/Used/Worn), Tape (Low/Std/High), Comp (Off/Lite/Heavy).

## Como compilar pelo GitHub
1. Suba esta pasta para um repositório.
2. Aba **Actions** -> workflow **Build TapeBox VST3** (roda sozinho a cada push).
3. Baixe o artefato do seu sistema (Windows, macOS ou Linux).
4. Para criar uma Release automática: `git tag v1.0.0 && git push --tags`.

## Instalar
- Windows: copie `TapeBox.vst3` para `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (se bloquear: `xattr -cr TapeBox.vst3`)
- Linux: `~/.vst3/`

## Compilar localmente
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Nota: o plugin reporta ~4 ms de latência (atraso base do wow/flutter).
