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

#ifndef SIMDCONVERTER_H
#define SIMDCONVERTER_H

#include <cstdint>
#include <string>

/**
 * SIMD-optimized float-to-int16 conversion utilities.
 * 
 * This class provides high-performance conversion from float to int16 with
 * scaling, using platform-specific SIMD instructions when available:
 * - ARM NEON (Apple Silicon, ARM64)
 * - x86 SSE2/SSE4.1 (Intel/AMD)
 * - Scalar fallback for unsupported platforms
 * 
 * The conversion performs: out[i] = clamp(round(in[i] * scale), -32768, 32767)
 * 
 * This fuses the two-pass approach used in BinaryRecording::writeContinuousData()
 * (FloatVectorOperations::copyWithMultiply + AudioDataConverters::convertFloatToInt16LE)
 * into a single pass, eliminating the intermediate buffer and improving cache utilization.
 */
class SIMDConverter
{
public:
    /**
     * Supported SIMD instruction sets
     */
    enum class SIMDType
    {
        None,       // Scalar fallback
        SSE2,       // x86 SSE2 (128-bit, 4 floats)
        SSE4_1,     // x86 SSE4.1 (adds packing instructions)
        AVX2,       // x86 AVX2 (256-bit, 8 floats) - future
        NEON        // ARM NEON (128-bit, 4 floats)
    };

    /**
     * Detects and returns the best available SIMD instruction set for this CPU.
     * Result is cached after first call.
     */
    static SIMDType getAvailableSIMD();

    /**
     * Returns a human-readable string describing the active SIMD type.
     */
    static std::string getSIMDTypeString();

    /**
     * Converts float samples to int16 with scaling in a single fused operation.
     * 
     * Performs: output[i] = clamp(round(input[i] * scaleFactor), -32768, 32767)
     * 
     * @param input       Pointer to input float samples (does not need to be aligned)
     * @param output      Pointer to output int16 samples (does not need to be aligned)
     * @param scaleFactor Scale factor to apply (typically 1.0 / (32767.0 * bitVolts))
     * @param numSamples  Number of samples to convert
     */
    static void convertFloatToInt16 (const float* input,
                                     int16_t* output,
                                     float scaleFactor,
                                     int numSamples);

    /**
     * Batch conversion for multiple channels with potentially different scale factors.
     * More efficient than calling convertFloatToInt16 multiple times due to
     * better instruction pipelining.
     * 
     * @param inputs       Array of pointers to input float buffers (one per channel)
     * @param outputs      Array of pointers to output int16 buffers (one per channel)
     * @param scaleFactors Array of scale factors (one per channel)
     * @param numChannels  Number of channels to convert
     * @param numSamples   Number of samples per channel
     */
    static void convertFloatToInt16Batch (const float* const* inputs,
                                          int16_t* const* outputs,
                                          const float* scaleFactors,
                                          int numChannels,
                                          int numSamples);

    /**
     * Interleaves data from multiple channel buffers into a single interleaved output buffer.
     * This is optimized for the memory access patterns in disk recording.
     * 
     * Output format: [ch0_s0, ch1_s0, ..., chN_s0, ch0_s1, ch1_s1, ..., chN_s1, ...]
     * 
     * @param channelData  Array of pointers to int16 data for each channel (contiguous per channel)
     * @param output       Pointer to output buffer (will contain interleaved data)
     * @param numChannels  Number of channels
     * @param numSamples   Number of samples per channel
     * @param tileSamples  Tile size for sample dimension (for cache blocking, 0 = auto)
     * @param tileChannels Tile size for channel dimension (for cache blocking, 0 = auto)
     */
    static void interleaveInt16 (const int16_t* const* channelData,
                                 int16_t* output,
                                 int numChannels,
                                 int numSamples,
                                 int tileSamples = 0,
                                 int tileChannels = 0);

    /**
     * Recommended tile sizes based on cache characteristics.
     * These are tuned for typical L1 cache sizes (32-64KB).
     */
    struct TileConfig
    {
        int tileSamples;    // Number of samples per tile
        int tileChannels;   // Number of channels per tile
    };

    /**
     * Returns recommended tile configuration for the given channel count.
     * Optimizes for L1 cache efficiency.
     */
    static TileConfig getRecommendedTileConfig (int numChannels);

private:
    // Implementation functions for each SIMD type
    static void convertScalar (const float* input, int16_t* output, float scale, int numSamples);
    
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    static void convertNEON (const float* input, int16_t* output, float scale, int numSamples);
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    static void convertSSE2 (const float* input, int16_t* output, float scale, int numSamples);
#endif

#if defined(__SSE4_1__)
    static void convertSSE4_1 (const float* input, int16_t* output, float scale, int numSamples);
#endif

    // Cached SIMD type detection result
    static SIMDType s_detectedSIMD;
    static bool s_simdDetected;
};

#endif // SIMDCONVERTER_H
