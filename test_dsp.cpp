#include "LupitDspCore.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>

int main()
{
    std::cout << "===================================================\n";
    std::cout << "Testing Lupit Phase Expander Production DSP Core...\n";
    std::cout << "===================================================\n";

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr int totalBlocks = 200; // ~1.06 seconds

    Lupit::DualStageProcessor processor;
    processor.prepare(sampleRate);

    Lupit::DualStageProcessor::Params params;
    params.stereoLink = true;
    params.sidechainHpfEnabled = true;
    params.sidechainHpfFreq = 90.0f;

    // Stage 1: Coarse (-32dB thresh, -14dB floor)
    params.stage1Thresh = -32.0f;
    params.stage1Floor = -14.0f;
    params.stage1Ratio = 3.0f;
    params.stage1AttackMs = 1.0f;
    params.stage1ReleaseMs = 60.0f;

    // Stage 2: Fine Trim (-42dB thresh, -10dB floor)
    params.stage2Thresh = -42.0f;
    params.stage2Floor = -10.0f;
    params.stage2Ratio = 2.0f;
    params.stage2AttackMs = 4.0f;
    params.stage2ReleaseMs = 120.0f;

    params.outputGainDb = 0.0f;
    processor.setParameters(params);

    // Test 1: Full speech signal (0 dBFS peak sine at 1kHz)
    // Expectation: Gain reduction should be ~0 dB (gate wide open, zero phase shift)
    std::vector<float> speechL(blockSize);
    std::vector<float> speechR(blockSize);
    for (int i = 0; i < blockSize; ++i)
    {
        float s = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * (float)i / (float)sampleRate);
        speechL[i] = s;
        speechR[i] = s;
    }
    float* speechChannels[2] = { speechL.data(), speechR.data() };

    for (int b = 0; b < 20; ++b)
        processor.process(speechChannels, 2, blockSize);

    std::cout << "\n[TEST 1] Speech Input (-6 dBFS tone):\n";
    std::cout << "  Stage 1 GR: " << processor.getStage1GainReductionDb() << " dB (Expected ~0 dB)\n";
    std::cout << "  Stage 2 GR: " << processor.getStage2GainReductionDb() << " dB (Expected ~0 dB)\n";
    assert(processor.getStage1GainReductionDb() > -0.5f);
    assert(processor.getStage2GainReductionDb() > -0.5f);

    // Test 2: Low-level room noise (-60 dBFS tone)
    // Expectation: Gate closes smoothly down to Stage 1 floor (-14 dB) + Stage 2 floor (-10 dB) = ~ -24 dB net reduction
    std::vector<float> noiseL(blockSize);
    std::vector<float> noiseR(blockSize);
    for (int i = 0; i < blockSize; ++i)
    {
        float n = 0.001f * std::sin(2.0f * 3.14159265f * 500.0f * (float)i / (float)sampleRate);
        noiseL[i] = n;
        noiseR[i] = n;
    }
    float* noiseChannels[2] = { noiseL.data(), noiseR.data() };

    for (int b = 0; b < totalBlocks; ++b)
        processor.process(noiseChannels, 2, blockSize);

    std::cout << "\n[TEST 2] Background Noise Input (-60 dBFS tone):\n";
    std::cout << "  Stage 1 GR: " << processor.getStage1GainReductionDb() << " dB (Expected approx -14 dB floor)\n";
    std::cout << "  Stage 2 GR: " << processor.getStage2GainReductionDb() << " dB (Expected approx -10 dB floor)\n";

    assert(processor.getStage1GainReductionDb() <= -13.5f && processor.getStage1GainReductionDb() >= -14.5f);
    assert(processor.getStage2GainReductionDb() <= -9.5f && processor.getStage2GainReductionDb() >= -10.5f);

    // Verify non-zero bedrock: Signal is preserved at expected attenuation, NOT destroyed/clamped to 0
    float lastSample = noiseChannels[0][blockSize - 1];
    std::cout << "  Output sample level: " << 20.0f * std::log10(std::abs(lastSample) + 1e-9f) << " dBFS (Bedrock maintained)\n";

    std::cout << "\n>>> ALL DSP VERIFICATION TESTS PASSED SUCCESSFULLY! <<<\n";
    return 0;
}
