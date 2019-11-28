cmake_minimum_required(VERSION 3.12)
project(MeltonJohn)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(MeltonJohn
        src/BeatDetect.cpp
        src/BeatDetect.hpp
        src/fftsg.cpp
        src/fftsg.hpp
        src/main.cpp
        src/Patterns.cpp
        src/PCM.cpp
        src/PCM.hpp
        src/MidiMix.cpp
        src/MidiMix.h
        )
target_link_libraries(MeltonJohn pulse pulse-simple)