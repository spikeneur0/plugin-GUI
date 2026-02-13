/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "LfpViewerProcessing.h"

#include <cmath>

// MSVC with /arch:AVX doesn't define __SSE4_1__, but AVX implies SSE4.1 support
// Normalize compiler feature macros so x86 SIMD detection is consistent
// across MSVC/GCC/Clang builds.
// MSVC with /arch:AVX doesn't define __SSE4_1__, but AVX implies SSE4.1 support.
#if defined(_MSC_VER) && defined(__AVX__) && !defined(__SSE4_1__)
 #define __SSE4_1__ 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
 #include <arm_neon.h>
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
 #include <emmintrin.h>
#endif

#if defined(__SSE4_1__)
 #include <smmintrin.h>
#endif

#if defined(_MSC_VER)
 #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(__x86_64__) || defined(__i386__)
  #include <cpuid.h>
 #endif
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
// Build-time availability flags for architecture-specific implementations.
// Runtime detection still selects the active path where applicable.
 #define LFPVIEWER_USE_SSE 1
#else
 #define LFPVIEWER_USE_SSE 0
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
 #define LFPVIEWER_USE_NEON 1
#else
 #define LFPVIEWER_USE_NEON 0
#endif

namespace
{
constexpr float highPassCutoffHz = 300.0f;
constexpr float highPassQ = 0.70710678f;
constexpr float pi = 3.14159265358979323846f;

// SIMD backend selection for LFP high-pass processing.
enum class HighPassSIMDType
{
    None,
    SSE2,
    SSE4_1,
    NEON
};

// Detects the best SIMD implementation to use for this process and caches it.
// This allows binaries compiled on one machine to adapt at runtime to the
// end-user CPU capabilities.
HighPassSIMDType getAvailableHighPassSIMD()
{
    static bool simdDetected = false;
    static HighPassSIMDType detectedSIMD = HighPassSIMDType::None;

    if (simdDetected)
        return detectedSIMD;

    simdDetected = true;

#if LFPVIEWER_USE_NEON
    // On ARM builds with NEON enabled, NEON is available by definition.
    detectedSIMD = HighPassSIMDType::NEON;
    return detectedSIMD;
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    // x86 path: query CPU feature bits (CPUID leaf 1).
    int cpuInfo[4] = { 0, 0, 0, 0 };

   #if defined(_MSC_VER)
    __cpuid (cpuInfo, 1);
   #elif defined(__GNUC__) || defined(__clang__)
    __cpuid (1, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
   #endif

    const bool hasSSE4_1 = (cpuInfo[2] & (1 << 19)) != 0;
    const bool hasSSE2 = (cpuInfo[3] & (1 << 26)) != 0;

   #if defined(__SSE4_1__)
    if (hasSSE4_1)
    {
        detectedSIMD = HighPassSIMDType::SSE4_1;
        return detectedSIMD;
    }
   #endif

   #if LFPVIEWER_USE_SSE
    if (hasSSE2)
    {
        detectedSIMD = HighPassSIMDType::SSE2;
        return detectedSIMD;
    }
   #endif
#endif

    detectedSIMD = HighPassSIMDType::None;
    return detectedSIMD;
}

// Scalar per-channel high-pass core (Transposed Direct Form II biquad).
// Used for single-channel processing and as fallback/tail handling.
inline void processHighPassScalar (float* data,
                                   int numSamples,
                                   float b0,
                                   float b1,
                                   float b2,
                                   float a1,
                                   float a2,
                                   float& z1,
                                   float& z2)
{
    for (int i = 0; i < numSamples; i++)
    {
        const float x = data[i];
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        data[i] = y;
    }
}
} // namespace

namespace LfpViewer
{

LfpViewerProcessing::LfpViewerProcessing()
{
}

LfpViewerProcessing::~LfpViewerProcessing()
{
}

void LfpViewerProcessing::prepare (int numChannels_, float sampleRate_,
                                   const Array<ContinuousChannel::Type>& channelTypes)
{
    numChannels = numChannels_;
    sampleRate = sampleRate_;

    isElectrodeChannel.clear();
    for (int i = 0; i < numChannels; i++)
    {
        isElectrodeChannel.add (i < channelTypes.size()
                                    ? channelTypes[i] == ContinuousChannel::Type::ELECTRODE
                                    : true);
    }

    createHighPassFilters();
    setupNeuropixelsGroups();
}

void LfpViewerProcessing::reset()
{
    std::fill (highPassZ1.begin(), highPassZ1.end(), 0.0f);
    std::fill (highPassZ2.begin(), highPassZ2.end(), 0.0f);
}

// ----- High-pass filter -----

void LfpViewerProcessing::setHighPassEnabled (bool enabled)
{
    highPassEnabled = enabled;
}

void LfpViewerProcessing::createHighPassFilters()
{
    // Allocate per-channel filter states and clear them.
    highPassZ1.assign ((size_t) jmax (0, numChannels), 0.0f);
    highPassZ2.assign ((size_t) jmax (0, numChannels), 0.0f);
    updateHighPassCoefficients();
}

void LfpViewerProcessing::updateHighPassCoefficients()
{
    // RBJ cookbook 2nd-order high-pass biquad, clamped to a stable range.
    const float minCutoff = 1.0f;
    const float maxCutoff = jmax (minCutoff, 0.45f * sampleRate);
    const float cutoff = jlimit (minCutoff, maxCutoff, highPassCutoffHz);

    const float w0 = 2.0f * pi * cutoff / sampleRate;
    const float cosW0 = std::cos (w0);
    const float sinW0 = std::sin (w0);
    const float alpha = sinW0 / (2.0f * highPassQ);

    const float b0 = (1.0f + cosW0) * 0.5f;
    const float b1 = -(1.0f + cosW0);
    const float b2 = (1.0f + cosW0) * 0.5f;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * cosW0;
    const float a2 = 1.0f - alpha;

    highPassB0 = b0 / a0;
    highPassB1 = b1 / a0;
    highPassB2 = b2 / a0;
    highPassA1 = a1 / a0;
    highPassA2 = a2 / a0;
}

void LfpViewerProcessing::applyHighPass (AudioBuffer<float>& buffer, int numSamples, int numCh)
{
    if (numSamples <= 0 || numCh <= 0)
        return;

    // Process only channels that have state allocated.
    const int maxCh = jmin (numCh, (int) highPassZ1.size());
    // Choose the best backend once per call.
    const auto simdType = getAvailableHighPassSIMD();

#if LFPVIEWER_USE_SSE
    if (simdType == HighPassSIMDType::SSE4_1 || simdType == HighPassSIMDType::SSE2)
    {
    // Lane-per-channel SIMD (4 channels at a time) using x86 SSE.
    constexpr int simdWidth = 4;

    const __m128 b0 = _mm_set1_ps (highPassB0);
    const __m128 b1 = _mm_set1_ps (highPassB1);
    const __m128 b2 = _mm_set1_ps (highPassB2);
    const __m128 a1 = _mm_set1_ps (highPassA1);
    const __m128 a2 = _mm_set1_ps (highPassA2);

    int ch = 0;
    for (; ch + simdWidth <= maxCh; ch += simdWidth)
    {
        // SIMD block is only valid if all channels in the block are electrode channels.
        bool allElectrode = true;
        for (int lane = 0; lane < simdWidth; lane++)
        {
            if (ch + lane >= isElectrodeChannel.size() || ! isElectrodeChannel[ch + lane])
            {
                allElectrode = false;
                break;
            }
        }

        if (! allElectrode)
            continue;

        // Gather channel pointers for lane-per-channel vector processing.
        float* d0 = buffer.getWritePointer (ch + 0);
        float* d1 = buffer.getWritePointer (ch + 1);
        float* d2 = buffer.getWritePointer (ch + 2);
        float* d3 = buffer.getWritePointer (ch + 3);

        __m128 z1 = _mm_set_ps (highPassZ1[(size_t) ch + 3],
                                highPassZ1[(size_t) ch + 2],
                                highPassZ1[(size_t) ch + 1],
                                highPassZ1[(size_t) ch + 0]);
        __m128 z2 = _mm_set_ps (highPassZ2[(size_t) ch + 3],
                                highPassZ2[(size_t) ch + 2],
                                highPassZ2[(size_t) ch + 1],
                                highPassZ2[(size_t) ch + 0]);

        alignas (16) float y[simdWidth];

        // Run one biquad update per sample, with channels mapped to SIMD lanes.
        for (int i = 0; i < numSamples; i++)
        {
            const __m128 x = _mm_set_ps (d3[i], d2[i], d1[i], d0[i]);
            const __m128 out = _mm_add_ps (_mm_mul_ps (b0, x), z1);
            z1 = _mm_add_ps (_mm_sub_ps (_mm_mul_ps (b1, x), _mm_mul_ps (a1, out)), z2);
            z2 = _mm_sub_ps (_mm_mul_ps (b2, x), _mm_mul_ps (a2, out));

            _mm_storeu_ps (y, out);
            d0[i] = y[0];
            d1[i] = y[1];
            d2[i] = y[2];
            d3[i] = y[3];
        }

        // Persist per-channel filter state back to scalar storage.
        alignas (16) float z1Out[simdWidth];
        alignas (16) float z2Out[simdWidth];
        _mm_storeu_ps (z1Out, z1);
        _mm_storeu_ps (z2Out, z2);

        highPassZ1[(size_t) ch + 0] = z1Out[0];
        highPassZ1[(size_t) ch + 1] = z1Out[1];
        highPassZ1[(size_t) ch + 2] = z1Out[2];
        highPassZ1[(size_t) ch + 3] = z1Out[3];

        highPassZ2[(size_t) ch + 0] = z2Out[0];
        highPassZ2[(size_t) ch + 1] = z2Out[1];
        highPassZ2[(size_t) ch + 2] = z2Out[2];
        highPassZ2[(size_t) ch + 3] = z2Out[3];
    }

    // Scalar cleanup for channels not covered by SIMD blocks.
    for (int chTail = 0; chTail < maxCh; chTail++)
    {
        if (chTail % simdWidth == 0 && chTail + simdWidth <= maxCh)
        {
            bool allElectrode = true;
            for (int lane = 0; lane < simdWidth; lane++)
            {
                if (chTail + lane >= isElectrodeChannel.size() || ! isElectrodeChannel[chTail + lane])
                {
                    allElectrode = false;
                    break;
                }
            }

            if (allElectrode)
            {
                chTail += simdWidth - 1;
                continue;
            }
        }

        if (chTail >= isElectrodeChannel.size() || ! isElectrodeChannel[chTail])
            continue;

        float* data = buffer.getWritePointer (chTail);
        processHighPassScalar (data,
                               numSamples,
                               highPassB0,
                               highPassB1,
                               highPassB2,
                               highPassA1,
                               highPassA2,
                               highPassZ1[(size_t) chTail],
                               highPassZ2[(size_t) chTail]);
    }
        return;
    }
#endif

#if LFPVIEWER_USE_NEON
    if (simdType == HighPassSIMDType::NEON)
    {
    // Lane-per-channel SIMD (4 channels at a time) using ARM NEON.
    constexpr int simdWidth = 4;

    const float32x4_t b0 = vdupq_n_f32 (highPassB0);
    const float32x4_t b1 = vdupq_n_f32 (highPassB1);
    const float32x4_t b2 = vdupq_n_f32 (highPassB2);
    const float32x4_t a1 = vdupq_n_f32 (highPassA1);
    const float32x4_t a2 = vdupq_n_f32 (highPassA2);

    int ch = 0;
    for (; ch + simdWidth <= maxCh; ch += simdWidth)
    {
        // SIMD block is only valid if all channels in the block are electrode channels.
        bool allElectrode = true;
        for (int lane = 0; lane < simdWidth; lane++)
        {
            if (ch + lane >= isElectrodeChannel.size() || ! isElectrodeChannel[ch + lane])
            {
                allElectrode = false;
                break;
            }
        }

        if (! allElectrode)
            continue;

        // Gather channel pointers for lane-per-channel vector processing.
        float* d0 = buffer.getWritePointer (ch + 0);
        float* d1 = buffer.getWritePointer (ch + 1);
        float* d2 = buffer.getWritePointer (ch + 2);
        float* d3 = buffer.getWritePointer (ch + 3);

        float32x4_t z1 = { highPassZ1[(size_t) ch + 0],
                           highPassZ1[(size_t) ch + 1],
                           highPassZ1[(size_t) ch + 2],
                           highPassZ1[(size_t) ch + 3] };
        float32x4_t z2 = { highPassZ2[(size_t) ch + 0],
                           highPassZ2[(size_t) ch + 1],
                           highPassZ2[(size_t) ch + 2],
                           highPassZ2[(size_t) ch + 3] };

        alignas (16) float y[simdWidth];

        // Run one biquad update per sample, with channels mapped to SIMD lanes.
        for (int i = 0; i < numSamples; i++)
        {
            const float32x4_t x = { d0[i], d1[i], d2[i], d3[i] };
            const float32x4_t out = vmlaq_f32 (z1, b0, x);
            z1 = vaddq_f32 (vsubq_f32 (vmulq_f32 (b1, x), vmulq_f32 (a1, out)), z2);
            z2 = vsubq_f32 (vmulq_f32 (b2, x), vmulq_f32 (a2, out));

            vst1q_f32 (y, out);
            d0[i] = y[0];
            d1[i] = y[1];
            d2[i] = y[2];
            d3[i] = y[3];
        }

        // Persist per-channel filter state back to scalar storage.
        alignas (16) float z1Out[simdWidth];
        alignas (16) float z2Out[simdWidth];
        vst1q_f32 (z1Out, z1);
        vst1q_f32 (z2Out, z2);

        highPassZ1[(size_t) ch + 0] = z1Out[0];
        highPassZ1[(size_t) ch + 1] = z1Out[1];
        highPassZ1[(size_t) ch + 2] = z1Out[2];
        highPassZ1[(size_t) ch + 3] = z1Out[3];

        highPassZ2[(size_t) ch + 0] = z2Out[0];
        highPassZ2[(size_t) ch + 1] = z2Out[1];
        highPassZ2[(size_t) ch + 2] = z2Out[2];
        highPassZ2[(size_t) ch + 3] = z2Out[3];
    }

    // Scalar cleanup for channels not covered by SIMD blocks.
    for (int chTail = 0; chTail < maxCh; chTail++)
    {
        if (chTail % simdWidth == 0 && chTail + simdWidth <= maxCh)
        {
            bool allElectrode = true;
            for (int lane = 0; lane < simdWidth; lane++)
            {
                if (chTail + lane >= isElectrodeChannel.size() || ! isElectrodeChannel[chTail + lane])
                {
                    allElectrode = false;
                    break;
                }
            }

            if (allElectrode)
            {
                chTail += simdWidth - 1;
                continue;
            }
        }

        if (chTail >= isElectrodeChannel.size() || ! isElectrodeChannel[chTail])
            continue;

        float* data = buffer.getWritePointer (chTail);
        processHighPassScalar (data,
                               numSamples,
                               highPassB0,
                               highPassB1,
                               highPassB2,
                               highPassA1,
                               highPassA2,
                               highPassZ1[(size_t) chTail],
                               highPassZ2[(size_t) chTail]);
    }
        return;
    }
#endif

    // Generic scalar fallback for unsupported architectures/feature sets.
    for (int ch = 0; ch < maxCh; ch++)
    {
        if (ch >= isElectrodeChannel.size() || ! isElectrodeChannel[ch])
            continue;

        float* data = buffer.getWritePointer (ch);
        processHighPassScalar (data,
                               numSamples,
                               highPassB0,
                               highPassB1,
                               highPassB2,
                               highPassA1,
                               highPassA2,
                               highPassZ1[(size_t) ch],
                               highPassZ2[(size_t) ch]);
    }
}

// ----- Common Average Reference -----

void LfpViewerProcessing::setCAREnabled (bool enabled)
{
    carEnabled = enabled;
}

void LfpViewerProcessing::setNeuropixelsAdcCount (int adcCount)
{
    numAdcs = adcCount;
    setupNeuropixelsGroups();
}

void LfpViewerProcessing::setupNeuropixelsGroups()
{
    channelGroups.clear();
    channelCounts.clear();
    numGroups = 0;

    if (numAdcs == 0)
    {
        // Standard CAR: one group for all electrode channels
        numGroups = 1;
        carAvgBuffer.setSize (1, 10000);
        carAvgBuffer.clear();
        return;
    }

    // Neuropixels ADC-group CAR
    // Replicate the grouping from the Neuropixels CAR plugin
    if (numAdcs == 32)
    {
        // NP 1.0: 12 groups
        for (int i = 0; i < jmin (numChannels, 384); i++)
        {
            channelGroups.add ((i / 2) % 12);
        }

        numGroups = 12;
        channelCounts.insertMultiple (0, 0.0f, 12);
        carAvgBuffer.setSize (12, 10000);
    }
    else if (numAdcs == 24)
    {
        // NP 2.0: 16 groups
        for (int i = 0; i < jmin (numChannels, 384); i++)
        {
            channelGroups.add ((i / 2) % 16);
        }

        numGroups = 16;
        channelCounts.insertMultiple (0, 0.0f, 16);
        carAvgBuffer.setSize (16, 10000);
    }
    else
    {
        // Unknown ADC count: fall back to standard CAR
        numAdcs = 0;
        numGroups = 1;
        carAvgBuffer.setSize (1, 10000);
    }

    carAvgBuffer.clear();
}

void LfpViewerProcessing::applyCAR (AudioBuffer<float>& buffer, int numSamples, int numCh)
{
    if (numSamples <= 0 || numCh <= 0)
        return;

    if (isNeuropixelsMode())
        applyNeuropixelsCAR (buffer, numSamples, numCh);
    else
        applyStandardCAR (buffer, numSamples, numCh);
}

void LfpViewerProcessing::applyStandardCAR (AudioBuffer<float>& buffer, int numSamples, int numCh)
{
    // Ensure the avg buffer is large enough
    if (carAvgBuffer.getNumSamples() < numSamples)
        carAvgBuffer.setSize (1, numSamples, false, false, true);

    carAvgBuffer.clear (0, 0, numSamples);

    // Sum all electrode channels
    int refCount = 0;
    for (int ch = 0; ch < numCh; ch++)
    {
        if (ch < isElectrodeChannel.size() && isElectrodeChannel[ch])
        {
            carAvgBuffer.addFrom (0, 0, buffer.getReadPointer (ch), numSamples);
            refCount++;
        }
    }

    if (refCount == 0)
        return;

    // Compute the mean
    carAvgBuffer.applyGain (0, 0, numSamples, 1.0f / float (refCount));

    // Subtract the mean from each electrode channel
    for (int ch = 0; ch < numCh; ch++)
    {
        if (ch < isElectrodeChannel.size() && isElectrodeChannel[ch])
        {
            buffer.addFrom (ch, 0, carAvgBuffer.getReadPointer (0), numSamples, -1.0f);
        }
    }
}

void LfpViewerProcessing::applyNeuropixelsCAR (AudioBuffer<float>& buffer, int numSamples, int numCh)
{
    if (numGroups <= 0)
        return;

    // Ensure the avg buffer is large enough
    if (carAvgBuffer.getNumSamples() < numSamples)
        carAvgBuffer.setSize (numGroups, numSamples, false, false, true);

    carAvgBuffer.clear();

    // Reset channel counts
    for (int g = 0; g < channelCounts.size(); g++)
        channelCounts.set (g, 0.0f);

    // Sum values per group
    const int maxCh = jmin (numCh, channelGroups.size());
    for (int ch = 0; ch < maxCh; ch++)
    {
        if (ch < isElectrodeChannel.size() && ! isElectrodeChannel[ch])
            continue;

        int group = channelGroups[ch];
        carAvgBuffer.addFrom (group, 0, buffer.getReadPointer (ch), numSamples);
        channelCounts.set (group, channelCounts[group] + 1.0f);
    }

    // Compute per-group mean
    for (int g = 0; g < numGroups; g++)
    {
        if (channelCounts[g] > 0)
            carAvgBuffer.applyGain (g, 0, numSamples, 1.0f / channelCounts[g]);
    }

    // Subtract the group mean from each channel
    for (int ch = 0; ch < maxCh; ch++)
    {
        if (ch < isElectrodeChannel.size() && ! isElectrodeChannel[ch])
            continue;

        int group = channelGroups[ch];
        buffer.addFrom (ch, 0, carAvgBuffer.getReadPointer (group), numSamples, -1.0f);
    }
}

} // namespace LfpViewer
