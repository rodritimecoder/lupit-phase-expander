#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

/**
 * @file LupitDspCore.h
 * @brief Production-grade DSP Core for the Lupit Dual-Stage Phase Expander / Suppressor.
 *
 * Implements:
 * 1. Linked or independent multi-channel processing.
 * 2. 2nd-order Butterworth High-Pass sidechain filter (eliminates plosive/sub-rumble gate pumping).
 * 3. Log-domain envelope tracking with separate attack/release ballistic smoothing.
 * 4. Quadratic soft-knee downward expander transfer curve with bedrock floor clamping.
 * 5. Gain reduction ballistic smoother to prevent low-frequency modulation distortion / clicks.
 * 6. Dual cascaded stages (Stage 1 Coarse, Stage 2 Fine).
 */

namespace Lupit
{

// Utility functions
inline float dbToGain(float db) noexcept
{
    return std::pow(10.0f, db * 0.05f);
}

inline float gainToDb(float gain) noexcept
{
    return (gain > 1.0e-6f) ? 20.0f * std::log10(gain) : -120.0f;
}

/**
 * Direct Form II Transposed 2nd-Order IIR Highpass Filter for detector sidechain.
 */
class SidechainHighpass
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = static_cast<float>(sampleRate);
        setCutoff(cutoffHz);
        reset();
    }

    void reset() noexcept
    {
        s1 = 0.0f;
        s2 = 0.0f;
    }

    void setCutoff(float cutoffFrequencyHz) noexcept
    {
        cutoffHz = std::clamp(cutoffFrequencyHz, 10.0f, 0.45f * fs);
        const float omega = 2.0f * 3.14159265358979323846f * (cutoffHz / fs);
        const float sn = std::sin(omega);
        const float cs = std::cos(omega);
        const float alpha = sn * 0.7071067811865475f; // Q = 1/sqrt(2) Butterworth

        const float a0 = 1.0f + alpha;
        const float invA0 = 1.0f / a0;

        b0 = ((1.0f + cs) * 0.5f) * invA0;
        b1 = (-(1.0f + cs)) * invA0;
        b2 = ((1.0f + cs) * 0.5f) * invA0;
        a1 = (-2.0f * cs) * invA0;
        a2 = (1.0f - alpha) * invA0;
    }

    inline float processSample(float in) noexcept
    {
        const float out = b0 * in + s1;
        s1 = b1 * in - a1 * out + s2;
        s2 = b2 * in - a2 * out;
        return out;
    }

private:
    float fs = 44100.0f;
    float cutoffHz = 80.0f;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
};

/**
 * Single Downward Expander Stage with soft knee and gain smoothing.
 */
class ExpanderStage
{
public:
    void prepare(double sampleRate) noexcept
    {
        fs = static_cast<float>(sampleRate);
        reset();
        updateCoeffs();
    }

    void reset() noexcept
    {
        detectorEnvDb = -120.0f;
        smoothedGainReduction = 1.0f;
    }

    void setParameters(float thresholdDbIn,
                       float floorDbIn,
                       float ratioIn,
                       float attackMsIn,
                       float releaseMsIn,
                       float kneeWidthDbIn = 4.0f) noexcept
    {
        threshDb = thresholdDbIn;
        floorDb = floorDbIn; // e.g. -14.0 dB
        ratio = std::max(1.0f, ratioIn);
        attackMs = std::max(0.05f, attackMsIn);
        releaseMs = std::max(1.0f, releaseMsIn);
        kneeWidthDb = std::max(0.0f, kneeWidthDbIn);
        updateCoeffs();
    }

    /**
     * Calculates the static gain reduction target in dB given a sidechain detector level in dB.
     * Incorporates quadratic soft knee around threshold and clamps to floorDb.
     */
    inline float calculateTargetGainReductionDb(float detectorDb) const noexcept
    {
        const float halfKnee = kneeWidthDb * 0.5f;

        // Above knee: 0 dB gain reduction (fully open)
        if (detectorDb >= threshDb + halfKnee)
            return 0.0f;

        // Below knee: linear expansion slope
        if (detectorDb <= threshDb - halfKnee)
        {
            const float grLinear = (detectorDb - threshDb) * (ratio - 1.0f);
            return std::max(floorDb, grLinear);
        }

        // Inside knee: smooth quadratic interpolation
        const float delta = detectorDb - threshDb + halfKnee;
        const float kneeFactor = (ratio - 1.0f) / (2.0f * kneeWidthDb);
        const float grKnee = -1.0f * (delta - kneeWidthDb) * delta * kneeFactor;

        return std::max(floorDb, grKnee);
    }

    /**
     * Updates detector envelope given the absolute peak level of the detector signal.
     */
    inline float updateDetectorDb(float detLevel) noexcept
    {
        const float sampleDb = (detLevel > 1.0e-6f) ? 20.0f * std::log10(detLevel) : -120.0f;

        if (sampleDb > detectorEnvDb)
            detectorEnvDb = attCoeff * detectorEnvDb + (1.0f - attCoeff) * sampleDb;
        else
            detectorEnvDb = relCoeff * detectorEnvDb + (1.0f - relCoeff) * sampleDb;

        return detectorEnvDb;
    }

    /**
     * Smooths gain reduction transition towards linear target factor.
     */
    inline float updateGainReduction(float targetLinearGain) noexcept
    {
        // Smooth gain transition with fast tracking to eliminate audio-rate modulation buzz
        smoothedGainReduction = gainSmoothCoeff * smoothedGainReduction + (1.0f - gainSmoothCoeff) * targetLinearGain;
        return smoothedGainReduction;
    }

    inline float getCurrentGainReductionLinear() const noexcept
    {
        return smoothedGainReduction;
    }

private:
    void updateCoeffs() noexcept
    {
        // Detector ballistics in dB domain
        attCoeff = std::exp(-1.0f / (fs * (attackMs * 0.001f)));
        relCoeff = std::exp(-1.0f / (fs * (releaseMs * 0.001f)));

        // Gain smoothing filter: ~0.5ms time constant eliminates zipper noise & LF harmonic distortion
        constexpr float gainSmoothTimeMs = 0.5f;
        gainSmoothCoeff = std::exp(-1.0f / (fs * (gainSmoothTimeMs * 0.001f)));
    }

    float fs = 44100.0f;
    float threshDb = -32.0f;
    float floorDb = -14.0f;
    float ratio = 2.5f;
    float attackMs = 1.5f;
    float releaseMs = 80.0f;
    float kneeWidthDb = 4.0f;

    float attCoeff = 0.0f;
    float relCoeff = 0.0f;
    float gainSmoothCoeff = 0.0f;

    float detectorEnvDb = -120.0f;
    float smoothedGainReduction = 1.0f;
};

/**
 * Complete Dual-Stage Lupit Phase Expander Engine.
 * Supports Stereo processing with stereo linking and optional high-pass sidechain filtering.
 */
class DualStageProcessor
{
public:
    struct Params
    {
        bool sidechainHpfEnabled = true;
        float sidechainHpfFreq = 90.0f;    // Hz
        bool stereoLink = true;

        // Stage 1 (Coarse)
        float stage1Thresh = -32.0f;       // dB
        float stage1Floor = -14.0f;        // dB
        float stage1Ratio = 2.5f;
        float stage1AttackMs = 1.5f;       // ms
        float stage1ReleaseMs = 80.0f;     // ms
        float stage1KneeDb = 4.0f;         // dB

        // Stage 2 (Fine Trim)
        float stage2Thresh = -42.0f;       // dB
        float stage2Floor = -10.0f;        // dB
        float stage2Ratio = 2.0f;
        float stage2AttackMs = 4.0f;       // ms
        float stage2ReleaseMs = 160.0f;    // ms
        float stage2KneeDb = 6.0f;         // dB

        float outputGainDb = 0.0f;         // dB
    };

    void prepare(double sampleRate) noexcept
    {
        fs = sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            sidechainHpf[ch].prepare(sampleRate);
            stage1[ch].prepare(sampleRate);
            stage2[ch].prepare(sampleRate);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            sidechainHpf[ch].reset();
            stage1[ch].reset();
            stage2[ch].reset();
        }
        meterGr1Db = 0.0f;
        meterGr2Db = 0.0f;
    }

    void setParameters(const Params& p) noexcept
    {
        params = p;
        for (int ch = 0; ch < 2; ++ch)
        {
            sidechainHpf[ch].setCutoff(p.sidechainHpfFreq);

            stage1[ch].setParameters(p.stage1Thresh, p.stage1Floor, p.stage1Ratio,
                                     p.stage1AttackMs, p.stage1ReleaseMs, p.stage1KneeDb);

            stage2[ch].setParameters(p.stage2Thresh, p.stage2Floor, p.stage2Ratio,
                                     p.stage2AttackMs, p.stage2ReleaseMs, p.stage2KneeDb);
        }
        outLinearGain = dbToGain(p.outputGainDb);
    }

    /**
     * Process an interleaved or block buffer.
     * @param channelData Array of pointers to channel audio buffers (float* [numChannels])
     * @param numChannels 1 (Mono) or 2 (Stereo)
     * @param numSamples Number of samples in buffer
     */
    void process(float* const* channelData, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;

        const bool isStereo = (numChannels >= 2);
        float minGr1 = 1.0f;
        float minGr2 = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = channelData[0][i];
            float inR = isStereo ? channelData[1][i] : inL;

            // Sidechain signal (filtered if enabled)
            float scL = params.sidechainHpfEnabled ? sidechainHpf[0].processSample(inL) : inL;
            float scR = isStereo ? (params.sidechainHpfEnabled ? sidechainHpf[1].processSample(inR) : inR) : scL;

            // Detector peak level
            float detL = std::abs(scL);
            float detR = std::abs(scR);

            if (params.stereoLink && isStereo)
            {
                const float maxDet = std::max(detL, detR);
                detL = maxDet;
                detR = maxDet;
            }

            // === STAGE 1 (Coarse) ===
            const float envDb1_L = stage1[0].updateDetectorDb(detL);
            const float targetGrDb1_L = stage1[0].calculateTargetGainReductionDb(envDb1_L);
            const float gr1_L = stage1[0].updateGainReduction(dbToGain(targetGrDb1_L));

            float gr1_R = gr1_L;
            if (isStereo && !params.stereoLink)
            {
                const float envDb1_R = stage1[1].updateDetectorDb(detR);
                const float targetGrDb1_R = stage1[1].calculateTargetGainReductionDb(envDb1_R);
                gr1_R = stage1[1].updateGainReduction(dbToGain(targetGrDb1_R));
            }

            // Apply Stage 1 attenuation
            float s1OutL = inL * gr1_L;
            float s1OutR = inR * gr1_R;

            // === STAGE 2 (Fine Trim) ===
            // Detection for Stage 2 operates on the output of Stage 1
            float sc2L = params.sidechainHpfEnabled ? sidechainHpf[0].processSample(s1OutL) : s1OutL;
            float sc2R = isStereo ? (params.sidechainHpfEnabled ? sidechainHpf[1].processSample(s1OutR) : s1OutR) : sc2L;

            float det2L = std::abs(sc2L);
            float det2R = std::abs(sc2R);

            if (params.stereoLink && isStereo)
            {
                const float maxDet2 = std::max(det2L, det2R);
                det2L = maxDet2;
                det2R = maxDet2;
            }

            const float envDb2_L = stage2[0].updateDetectorDb(det2L);
            const float targetGrDb2_L = stage2[0].calculateTargetGainReductionDb(envDb2_L);
            const float gr2_L = stage2[0].updateGainReduction(dbToGain(targetGrDb2_L));

            float gr2_R = gr2_L;
            if (isStereo && !params.stereoLink)
            {
                const float envDb2_R = stage2[1].updateDetectorDb(det2R);
                const float targetGrDb2_R = stage2[1].calculateTargetGainReductionDb(envDb2_R);
                gr2_R = stage2[1].updateGainReduction(dbToGain(targetGrDb2_R));
            }

            // Apply Stage 2 attenuation + Output makeup
            channelData[0][i] = s1OutL * gr2_L * outLinearGain;
            if (isStereo)
                channelData[1][i] = s1OutR * gr2_R * outLinearGain;

            minGr1 = std::min(minGr1, gr1_L);
            minGr2 = std::min(minGr2, gr2_L);
        }

        // Store peak gain reduction for UI meters
        meterGr1Db = gainToDb(minGr1);
        meterGr2Db = gainToDb(minGr2);
    }

    float getStage1GainReductionDb() const noexcept { return meterGr1Db; }
    float getStage2GainReductionDb() const noexcept { return meterGr2Db; }

private:
    double fs = 44100.0;
    Params params;
    float outLinearGain = 1.0f;

    SidechainHighpass sidechainHpf[2];
    ExpanderStage stage1[2];
    ExpanderStage stage2[2];

    float meterGr1Db = 0.0f;
    float meterGr2Db = 0.0f;
};

} // namespace Lupit
