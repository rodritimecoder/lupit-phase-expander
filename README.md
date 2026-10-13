# Lupit Phase Expander VST3 Project

A real-time, zero-latency dual-stage downward expander and vocal background suppressor built from the Lupit Phase Inversion methodology.

## 🚀 Key Architectural Improvements Over the Original Spec

1. **Direct Gain Scaling (Zero Redundant Summation):**
   * Replaced explicit phase buffer negation `x + (-x * (1 - G))` with direct gain attenuation `x * G`. This eliminates floating-point precision cancellation loss and CPU memory copying while producing identical results.
2. **Quadratic Soft-Knee Transitions:**
   * Prevents harsh corner transitions at the threshold cutoff.
3. **Audio-Rate Modulation Ballistic Smoothing (~0.5 ms):**
   * Eliminates low-frequency buzzing and clicks caused by raw peak detector modulation on vocal vowel fundamentals ($80\text{–}250\text{ Hz}$).
4. **Sidechain High-Pass Filter (2nd-Order Butterworth, 90 Hz):**
   * Prevents sub-bass rumble, microphone handling noise, and heavy plosives ('P', 'B') from inappropriately bursting the expander open.
5. **Full Stereo Linking:**
   * Maximum-peak linked detection across Left and Right channels prevents stereo image shifting.
6. **Double Non-Zero Bedrock Floor:**
   * Stage 1 clamps at `-14 dB`, Stage 2 clamps at `-10 dB`. Room ambience remains natural and never cuts to vacuum silence.

---

## 📌 Technical Note: JUCE 7 vs JUCE 8 Compatibility

We intentionally lock our build to **JUCE 7.0.12** instead of JUCE 8 for several critical industry reasons:
1. **API Stability & Compatibility:** JUCE 8 introduced breaking changes in font rendering (`juce_graphics` DirectWrite/FreeType abstractions), changes in `WebBrowserComponent`, altered default CMake module behaviors, and bumped C++ requirement standards. JUCE 7.0.12 is the battle-tested standard that the vast majority of shipping commercial plugins and open-source audio developers use.
2. **Licensing & Telemetry Changes:** JUCE 8 introduced a new tiered licensing scheme and updated telemetry terms that require login/tracking for certain tiers. JUCE 7 remains free under GPLv3 / ISC permissiveness with zero account verification friction.
3. **C++17 Compatibility:** JUCE 7 compiles cleanly across all toolchains (GCC 11+, Clang 14+, MSVC 2019+) without requiring newer C++20 standard library features.

---

## 🛠️ Building & Compiling

### 1. Standalone Verification Test
Verify the DSP engine directly with `g++` (no external dependencies needed):
```bash
cd /home/rodri/Projects/LupitPhaseExpander
g++ -O3 -std=c++17 test_dsp.cpp -o test_dsp
./test_dsp
```

### 2. Building the VST3 and Standalone Plugin with JUCE & CMake
```bash
cd /home/rodri/Projects/LupitPhaseExpander
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

The compiled binaries will be generated under:
* `build/LupitPhaseExpander_artefacts/Release/VST3/`
* `build/LupitPhaseExpander_artefacts/Release/Standalone/`
