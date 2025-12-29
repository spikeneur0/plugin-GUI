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

#include "SIMDConverter.h"

#include <algorithm>
#include <cmath>

// Platform-specific includes
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    #include <emmintrin.h>  // SSE2
#endif

#if defined(__SSE4_1__)
    #include <smmintrin.h>  // SSE4.1
#endif

// For CPUID on x86
#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__x86_64__) || defined(__i386__)
        #include <cpuid.h>
    #endif
#endif

// Static member initialization
SIMDConverter::SIMDType SIMDConverter::s_detectedSIMD = SIMDType::None;
bool SIMDConverter::s_simdDetected = false;

SIMDConverter::SIMDType SIMDConverter::getAvailableSIMD()
{
    if (s_simdDetected)
        return s_detectedSIMD;

    s_simdDetected = true;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    // ARM NEON is always available when compiled with NEON support
    s_detectedSIMD = SIMDType::NEON;
    return s_detectedSIMD;
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    // Runtime detection for x86 using CPUID
    int cpuInfo[4] = { 0 };

    #if defined(_MSC_VER)
        __cpuid(cpuInfo, 1);
    #elif defined(__GNUC__) || defined(__clang__)
        __cpuid(1, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
    #endif

    // Check SSE4.1 (bit 19 of ECX)
    bool hasSSE4_1 = (cpuInfo[2] & (1 << 19)) != 0;
    // Check SSE2 (bit 26 of EDX)
    bool hasSSE2 = (cpuInfo[3] & (1 << 26)) != 0;

    #if defined(__SSE4_1__)
    if (hasSSE4_1)
    {
        s_detectedSIMD = SIMDType::SSE4_1;
        return s_detectedSIMD;
    }
    #endif

    #if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    if (hasSSE2)
    {
        s_detectedSIMD = SIMDType::SSE2;
        return s_detectedSIMD;
    }
    #endif
#endif

    s_detectedSIMD = SIMDType::None;
    return s_detectedSIMD;
}

std::string SIMDConverter::getSIMDTypeString()
{
    switch (getAvailableSIMD())
    {
        case SIMDType::NEON:   return "ARM NEON";
        case SIMDType::SSE4_1: return "x86 SSE4.1";
        case SIMDType::SSE2:   return "x86 SSE2";
        case SIMDType::AVX2:   return "x86 AVX2";
        case SIMDType::None:   return "Scalar (no SIMD)";
        default:               return "Unknown";
    }
}

// ============================================================================
// Scalar implementation (fallback)
// ============================================================================

void SIMDConverter::convertScalar (const float* input, int16_t* output, float scale, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Scale, round, and clamp in one operation
        float scaled = input[i] * scale;
        int32_t rounded = static_cast<int32_t> (std::round (scaled));
        // Clamp to int16 range
        rounded = std::max (-32768, std::min (32767, rounded));
        output[i] = static_cast<int16_t> (rounded);
    }
}

// ============================================================================
// ARM NEON implementation
// ============================================================================

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
void SIMDConverter::convertNEON (const float* input, int16_t* output, float scale, int numSamples)
{
    const float32x4_t vscale = vdupq_n_f32 (scale);

    int i = 0;
    
    // Process 8 samples at a time (2 x float32x4 -> 1 x int16x8)
    const int simdWidth = 8;
    const int numFullIterations = numSamples / simdWidth;
    
    for (int iter = 0; iter < numFullIterations; ++iter)
    {
        // Load 8 floats (2 x 4)
        float32x4_t f0 = vld1q_f32 (input + i);
        float32x4_t f1 = vld1q_f32 (input + i + 4);
        
        // Multiply by scale factor
        f0 = vmulq_f32 (f0, vscale);
        f1 = vmulq_f32 (f1, vscale);
        
        // Convert to int32 with rounding (vcvtnq_s32_f32 rounds to nearest)
        int32x4_t i0 = vcvtnq_s32_f32 (f0);
        int32x4_t i1 = vcvtnq_s32_f32 (f1);
        
        // Narrow to int16 with saturation (combines two int32x4 into one int16x8)
        int16x4_t lo = vqmovn_s32 (i0);
        int16x4_t hi = vqmovn_s32 (i1);
        int16x8_t result = vcombine_s16 (lo, hi);
        
        // Store 8 int16 values
        vst1q_s16 (output + i, result);
        
        i += simdWidth;
    }
    
    // Handle remaining samples with scalar code
    for (; i < numSamples; ++i)
    {
        float scaled = input[i] * scale;
        int32_t rounded = static_cast<int32_t> (std::round (scaled));
        rounded = std::max (-32768, std::min (32767, rounded));
        output[i] = static_cast<int16_t> (rounded);
    }
}
#endif

// ============================================================================
// x86 SSE2 implementation
// ============================================================================

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
void SIMDConverter::convertSSE2 (const float* input, int16_t* output, float scale, int numSamples)
{
    const __m128 vscale = _mm_set1_ps (scale);
    
    int i = 0;
    
    // Process 8 samples at a time (2 x 4 floats -> 8 int16s)
    const int simdWidth = 8;
    const int simdIterations = numSamples / simdWidth;
    
    for (int iter = 0; iter < simdIterations; ++iter)
    {
        // Load 8 floats (2 x 4)
        __m128 f0 = _mm_loadu_ps (input + i);
        __m128 f1 = _mm_loadu_ps (input + i + 4);
        
        // Multiply by scale factor
        f0 = _mm_mul_ps (f0, vscale);
        f1 = _mm_mul_ps (f1, vscale);
        
        // Convert to int32 (truncates toward zero, but we'll round first)
        // SSE2 doesn't have direct round-to-nearest, so we use a trick:
        // add/subtract 0.5 and truncate
        const __m128 half = _mm_set1_ps (0.5f);
        const __m128 negHalf = _mm_set1_ps (-0.5f);
        const __m128 zero = _mm_setzero_ps();
        
        // Select +0.5 for positive, -0.5 for negative
        __m128 sign0 = _mm_cmpge_ps (f0, zero);
        __m128 sign1 = _mm_cmpge_ps (f1, zero);
        
        __m128 offset0 = _mm_or_ps (_mm_and_ps (sign0, half), _mm_andnot_ps (sign0, negHalf));
        __m128 offset1 = _mm_or_ps (_mm_and_ps (sign1, half), _mm_andnot_ps (sign1, negHalf));
        
        f0 = _mm_add_ps (f0, offset0);
        f1 = _mm_add_ps (f1, offset1);
        
        // Convert to int32 (truncates)
        __m128i i0 = _mm_cvttps_epi32 (f0);
        __m128i i1 = _mm_cvttps_epi32 (f1);
        
        // Pack two int32x4 into one int16x8 with signed saturation
        __m128i result = _mm_packs_epi32 (i0, i1);
        
        // Store 8 int16 values
        _mm_storeu_si128 (reinterpret_cast<__m128i*> (output + i), result);
        
        i += simdWidth;
    }
    
    // Handle remaining samples with scalar code
    for (; i < numSamples; ++i)
    {
        float scaled = input[i] * scale;
        int32_t rounded = static_cast<int32_t> (std::round (scaled));
        rounded = std::max (-32768, std::min (32767, rounded));
        output[i] = static_cast<int16_t> (rounded);
    }
}
#endif

// ============================================================================
// x86 SSE4.1 implementation (uses roundps for proper rounding)
// ============================================================================

#if defined(__SSE4_1__)
void SIMDConverter::convertSSE4_1 (const float* input, int16_t* output, float scale, int numSamples)
{
    const __m128 vscale = _mm_set1_ps (scale);
    
    int i = 0;
    
    // Process 8 samples at a time
    const int simdWidth = 8;
    const int simdIterations = numSamples / simdWidth;
    
    for (int iter = 0; iter < simdIterations; ++iter)
    {
        // Load 8 floats (2 x 4)
        __m128 f0 = _mm_loadu_ps (input + i);
        __m128 f1 = _mm_loadu_ps (input + i + 4);
        
        // Multiply by scale factor
        f0 = _mm_mul_ps (f0, vscale);
        f1 = _mm_mul_ps (f1, vscale);
        
        // Round to nearest integer (SSE4.1)
        // _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC = 0x00 | 0x08 = 0x08
        f0 = _mm_round_ps (f0, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        f1 = _mm_round_ps (f1, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        
        // Convert to int32
        __m128i i0 = _mm_cvtps_epi32 (f0);
        __m128i i1 = _mm_cvtps_epi32 (f1);
        
        // Pack two int32x4 into one int16x8 with signed saturation
        __m128i result = _mm_packs_epi32 (i0, i1);
        
        // Store 8 int16 values
        _mm_storeu_si128 (reinterpret_cast<__m128i*> (output + i), result);
        
        i += simdWidth;
    }
    
    // Handle remaining samples with scalar code
    for (; i < numSamples; ++i)
    {
        float scaled = input[i] * scale;
        int32_t rounded = static_cast<int32_t> (std::round (scaled));
        rounded = std::max (-32768, std::min (32767, rounded));
        output[i] = static_cast<int16_t> (rounded);
    }
}
#endif

// ============================================================================
// Main dispatch function
// ============================================================================

void SIMDConverter::convertFloatToInt16 (const float* input,
                                         int16_t* output,
                                         float scaleFactor,
                                         int numSamples)
{
    if (numSamples <= 0)
        return;
        
    SIMDType simd = getAvailableSIMD();
    
    switch (simd)
    {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        case SIMDType::NEON:
            convertNEON (input, output, scaleFactor, numSamples);
            return;
#endif

#if defined(__SSE4_1__)
        case SIMDType::SSE4_1:
            convertSSE4_1 (input, output, scaleFactor, numSamples);
            return;
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
        case SIMDType::SSE2:
            convertSSE2 (input, output, scaleFactor, numSamples);
            return;
#endif

        default:
            convertScalar (input, output, scaleFactor, numSamples);
            return;
    }
}

// ============================================================================
// Batch conversion (multiple channels)
// ============================================================================

void SIMDConverter::convertFloatToInt16Batch (const float* const* inputs,
                                              int16_t* const* outputs,
                                              const float* scaleFactors,
                                              int numChannels,
                                              int numSamples)
{
    // For now, simply call single-channel conversion in a loop
    // Future optimization: process multiple channels in parallel using wider SIMD
    for (int ch = 0; ch < numChannels; ++ch)
    {
        convertFloatToInt16 (inputs[ch], outputs[ch], scaleFactors[ch], numSamples);
    }
}

// ============================================================================
// Interleaving implementation
// ============================================================================

SIMDConverter::TileConfig SIMDConverter::getRecommendedTileConfig (int numChannels)
{
    // Target: Keep working set in L1 cache (~32KB)
    // Working set = tileSamples * tileChannels * 2 bytes (int16)
    // Plus output buffer: tileSamples * numChannels * 2 bytes
    
    // Tuned based on benchmark results (TileSizeTuning_Interleaving test)
    // Larger tile channels (128) with larger tile samples perform better
    // due to better SIMD vector utilization and cache line efficiency
    
    TileConfig config;
    
    if (numChannels >= 1024)
    {
        // Very high channel count: use larger tiles for better throughput
        // Benchmark showed 256x64 is ~10% faster than 128x32 at 1536 channels
        config.tileSamples = 256;
        config.tileChannels = 128;
    }
    else if (numChannels >= 384)
    {
        // Neuropixels range: 256x128 provides best performance
        // ~2% better than 256x64 at 384-768 channels
        config.tileSamples = 256;
        config.tileChannels = 128;
    }
    else if (numChannels >= 64)
    {
        // Medium channel count: larger sample tiles
        config.tileSamples = 512;
        config.tileChannels = 64;
    }
    else
    {
        // Low channel count: process all channels together
        config.tileSamples = 1024;
        config.tileChannels = numChannels;
    }
    
    return config;
}

// Scalar interleaving (fallback and reference implementation)
static void interleaveScalar (const int16_t* const* channelData,
                              int16_t* output,
                              int numChannels,
                              int numSamples,
                              int tileSamples,
                              int tileChannels)
{
    // Process in tiles for cache efficiency
    for (int sampleTile = 0; sampleTile < numSamples; sampleTile += tileSamples)
    {
        int sampleEnd = std::min (sampleTile + tileSamples, numSamples);
        
        for (int channelTile = 0; channelTile < numChannels; channelTile += tileChannels)
        {
            int channelEnd = std::min (channelTile + tileChannels, numChannels);
            
            // Process this tile: samples in outer loop for sequential output writes
            for (int s = sampleTile; s < sampleEnd; s++)
            {
                int16_t* outPtr = output + s * numChannels + channelTile;
                
                for (int ch = channelTile; ch < channelEnd; ch++)
                {
                    *outPtr++ = channelData[ch][s];
                }
            }
        }
    }
}

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
// ARM NEON optimized interleaving
static void interleaveNEON (const int16_t* const* channelData,
                            int16_t* output,
                            int numChannels,
                            int numSamples,
                            int tileSamples,
                            int tileChannels)
{
    // Process in tiles for cache efficiency
    for (int sampleTile = 0; sampleTile < numSamples; sampleTile += tileSamples)
    {
        int sampleEnd = std::min (sampleTile + tileSamples, numSamples);
        
        for (int channelTile = 0; channelTile < numChannels; channelTile += tileChannels)
        {
            int channelEnd = std::min (channelTile + tileChannels, numChannels);
            int tileWidth = channelEnd - channelTile;
            
            // Process this tile
            for (int s = sampleTile; s < sampleEnd; s++)
            {
                int16_t* outPtr = output + s * numChannels + channelTile;
                int ch = channelTile;
                
                // Use NEON to copy 8 channels at a time when aligned
                // This is beneficial when tileWidth >= 8
                while (ch + 8 <= channelEnd)
                {
                    // Load 8 values from 8 different channel pointers
                    // Unfortunately, NEON doesn't have a gather instruction,
                    // so we load individually and combine
                    int16x8_t vals = {
                        channelData[ch + 0][s],
                        channelData[ch + 1][s],
                        channelData[ch + 2][s],
                        channelData[ch + 3][s],
                        channelData[ch + 4][s],
                        channelData[ch + 5][s],
                        channelData[ch + 6][s],
                        channelData[ch + 7][s]
                    };
                    
                    // Store 8 values contiguously to output
                    vst1q_s16 (outPtr, vals);
                    
                    outPtr += 8;
                    ch += 8;
                }
                
                // Handle remaining channels
                while (ch < channelEnd)
                {
                    *outPtr++ = channelData[ch][s];
                    ch++;
                }
            }
        }
    }
}
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
// x86 SSE2 optimized interleaving
static void interleaveSSE2 (const int16_t* const* channelData,
                            int16_t* output,
                            int numChannels,
                            int numSamples,
                            int tileSamples,
                            int tileChannels)
{
    // Process in tiles for cache efficiency
    for (int sampleTile = 0; sampleTile < numSamples; sampleTile += tileSamples)
    {
        int sampleEnd = std::min (sampleTile + tileSamples, numSamples);
        
        for (int channelTile = 0; channelTile < numChannels; channelTile += tileChannels)
        {
            int channelEnd = std::min (channelTile + tileChannels, numChannels);
            int tileWidth = channelEnd - channelTile;
            
            // Process this tile
            for (int s = sampleTile; s < sampleEnd; s++)
            {
                int16_t* outPtr = output + s * numChannels + channelTile;
                int ch = channelTile;
                
                // Use SSE2 to copy 8 channels at a time
                while (ch + 8 <= channelEnd)
                {
                    // Manually gather 8 int16 values and pack into __m128i
                    // SSE2 doesn't have gather, so we use scalar loads + set
                    __m128i vals = _mm_set_epi16 (
                        channelData[ch + 7][s],
                        channelData[ch + 6][s],
                        channelData[ch + 5][s],
                        channelData[ch + 4][s],
                        channelData[ch + 3][s],
                        channelData[ch + 2][s],
                        channelData[ch + 1][s],
                        channelData[ch + 0][s]
                    );
                    
                    // Store 8 values contiguously
                    _mm_storeu_si128 (reinterpret_cast<__m128i*> (outPtr), vals);
                    
                    outPtr += 8;
                    ch += 8;
                }
                
                // Handle remaining channels
                while (ch < channelEnd)
                {
                    *outPtr++ = channelData[ch][s];
                    ch++;
                }
            }
        }
    }
}
#endif

void SIMDConverter::interleaveInt16 (const int16_t* const* channelData,
                                     int16_t* output,
                                     int numChannels,
                                     int numSamples,
                                     int tileSamples,
                                     int tileChannels)
{
    if (numSamples <= 0 || numChannels <= 0)
        return;
    
    // Use recommended tile sizes if not specified
    if (tileSamples <= 0 || tileChannels <= 0)
    {
        TileConfig config = getRecommendedTileConfig (numChannels);
        if (tileSamples <= 0)
            tileSamples = config.tileSamples;
        if (tileChannels <= 0)
            tileChannels = config.tileChannels;
    }
    
    // Clamp tile sizes to actual dimensions
    tileSamples = std::min (tileSamples, numSamples);
    tileChannels = std::min (tileChannels, numChannels);
    
    SIMDType simd = getAvailableSIMD();
    
    switch (simd)
    {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        case SIMDType::NEON:
            interleaveNEON (channelData, output, numChannels, numSamples, tileSamples, tileChannels);
            return;
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
        case SIMDType::SSE4_1:
        case SIMDType::SSE2:
            interleaveSSE2 (channelData, output, numChannels, numSamples, tileSamples, tileChannels);
            return;
#endif

        default:
            interleaveScalar (channelData, output, numChannels, numSamples, tileSamples, tileChannels);
            return;
    }
}
