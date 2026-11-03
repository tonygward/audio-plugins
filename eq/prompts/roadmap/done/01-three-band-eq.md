Create a JUCE-based C++ VST3 audio EQ plugin for Windows.

JUCE is installed at C:\dev\JUCE.

Use CMake and Visual Studio's MSVC compiler.

Reference: https://www.youtube.com/watch?v=qbde2yOmi3E&t=374s

Requirements:
- Stereo audio effect
- A working 3-band parametric EQ
- Frequency, gain and Q controls
- Simple graphical interface
- DAW-automatable parameters
- VST3 output for Studio One
- No third-party DSP libraries

First create the project structure and CMakeLists.txt, then build and fix compilation errors.

Do not attempt advanced features yet.
