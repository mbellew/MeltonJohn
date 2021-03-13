cmake_minimum_required(VERSION 3.12)
project(MeltonJohn)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(MeltonJohn
        src/beat_data.cpp
        src/beat_data.h
        src/BeatDetect.cpp
        src/BeatDetect.hpp
        src/fftsg.cpp
        src/fftsg.hpp
        src/main.cpp
        src/MidiPatterns.cpp
        src/MultiLayerPatterns.cpp
        src/Patterns.h
        src/Patterns.cpp
        src/PCM.cpp
        src/PCM.hpp
        src/MidiMix.cpp
        src/MidiMix.h
        src/Renderer.cpp
        src/Renderer.h
        src/config.h)
target_link_libraries(MeltonJohn pulse pulse-simple)




project(TeensyMelton)
add_executable(TeensyMelton
        TeensyMelton/beat_data.cpp
        TeensyMelton/beat_data.h
        TeensyMelton/main.cpp
        TeensyMelton/MultiLayerPatterns.cpp
        TeensyMelton/Patterns.h
        TeensyMelton/Patterns.cpp
        TeensyMelton/Renderer.cpp
        TeensyMelton/Renderer.h
        TeensyMelton/config.h)