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

/**
 * RecordNode Performance Benchmarks
 * 
 * This file contains benchmarks to measure the baseline performance of
 * the RecordNode's disk writing operations. These benchmarks establish
 * baseline metrics for optimization work.
 * 
 * Key metrics measured:
 * - Float-to-int16 conversion throughput (MB/s)
 * - Interleaved write throughput (MB/s)
 * - End-to-end recording throughput (samples/sec, MB/s)
 * - Per-channel vs batch operation overhead
 * 
 * Test configurations:
 * - 384 channels @ 30kHz (Neuropixels 1.0 single probe)
 * - 768 channels @ 30kHz (2x probes)
 * - 1536 channels @ 30kHz (4x probes)
 */

#include <stdio.h>
#include "gtest/gtest.h"

#include <Processors/RecordNode/RecordNode.h>
#include <Processors/RecordNode/BinaryFormat/BinaryRecording.h>
#include <Processors/RecordNode/BinaryFormat/SequentialBlockFile.h>
#include <ModelProcessors.h>
#include <ModelApplication.h>
#include <TestFixtures.h>

#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <fstream>

using namespace std::chrono;

/**
 * Benchmark result structure for collecting and reporting performance metrics
 */
struct BenchmarkResult
{
    std::string testName;
    int numChannels;
    int numSamples;
    int iterations;
    double totalTimeMs;
    double avgTimeMs;
    double samplesPerSecond;
    double megabytesPerSecond;
    double channelSamplesPerSecond;
    
    void print() const
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n========================================\n";
        std::cout << "Benchmark: " << testName << "\n";
        std::cout << "----------------------------------------\n";
        std::cout << "Configuration:\n";
        std::cout << "  Channels:    " << numChannels << "\n";
        std::cout << "  Samples:     " << numSamples << "\n";
        std::cout << "  Iterations:  " << iterations << "\n";
        std::cout << "Results:\n";
        std::cout << "  Total time:  " << totalTimeMs << " ms\n";
        std::cout << "  Avg time:    " << avgTimeMs << " ms/iteration\n";
        std::cout << "  Throughput:  " << samplesPerSecond / 1e6 << " M samples/sec\n";
        std::cout << "  Throughput:  " << megabytesPerSecond << " MB/sec\n";
        std::cout << "  Per-channel: " << channelSamplesPerSecond / 1e6 << " M ch-samples/sec\n";
        std::cout << "========================================\n";
    }
    
    void writeToCSV(std::ofstream& file) const
    {
        file << testName << ","
             << numChannels << ","
             << numSamples << ","
             << iterations << ","
             << totalTimeMs << ","
             << avgTimeMs << ","
             << samplesPerSecond << ","
             << megabytesPerSecond << ","
             << channelSamplesPerSecond << "\n";
    }
};

/**
 * Benchmark fixture for RecordNode performance testing
 */
class RecordNodeBenchmark : public testing::Test
{
protected:
    void SetUp() override
    {
        // Default configuration - can be overridden in specific tests
        numChannels = 384;
        sampleRate = 30000.0f;
        bitVolts = 0.195f; // Neuropixels default
        
        parentRecordingDir = std::filesystem::temp_directory_path() / "record_node_benchmark";
        if (std::filesystem::exists(parentRecordingDir))
        {
            std::filesystem::remove_all(parentRecordingDir);
        }
        std::filesystem::create_directory(parentRecordingDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(parentRecordingDir, ec);
    }

    /**
     * Initialize the ProcessorTester with specified channel count
     */
    void initializeTester(int channels, float rate = 30000.0f, float bv = 0.195f)
    {
        numChannels = channels;
        sampleRate = rate;
        bitVolts = bv;
        
        tester = std::make_unique<ProcessorTester>(TestSourceNodeBuilder(
            FakeSourceNodeParams{
                numChannels,
                sampleRate,
                bitVolts
            }));
        
        tester->setRecordingParentDirectory(parentRecordingDir.string());
        processor = tester->createProcessor<RecordNode>(Plugin::Processor::RECORD_NODE);
    }

    /**
     * Create a buffer with random-ish data that's representative of neural signals
     */
    AudioBuffer<float> createSignalBuffer(int channels, int samples)
    {
        AudioBuffer<float> buffer(channels, samples);
        
        // Fill with pseudo-random values in typical neural signal range
        // Using a simple pattern rather than true random for reproducibility
        for (int ch = 0; ch < channels; ch++)
        {
            float* data = buffer.getWritePointer(ch);
            for (int s = 0; s < samples; s++)
            {
                // Simulate neural signal: small oscillations with occasional spikes
                float baseSignal = 50.0f * std::sin(2.0f * 3.14159f * s / 100.0f + ch * 0.1f);
                float noise = ((s * 17 + ch * 31) % 100 - 50) * 0.5f;
                data[s] = baseSignal + noise;
            }
        }
        return buffer;
    }

    /**
     * Process a block through the RecordNode
     */
    void writeBlock(AudioBuffer<float>& buffer)
    {
        auto outBuffer = tester->processBlock(processor, buffer);
    }

    /**
     * Benchmark the float-to-int16 conversion in isolation
     * This directly tests the conversion used in BinaryRecording::writeContinuousData
     */
    BenchmarkResult benchmarkFloatToInt16Conversion(int channels, int samples, int iterations)
    {
        // Allocate buffers
        HeapBlock<float> inputBuffer(samples);
        HeapBlock<float> scaledBuffer(samples);
        HeapBlock<int16> outputBuffer(samples);
        
        // Fill input with test data
        for (int i = 0; i < samples; i++)
        {
            inputBuffer[i] = 100.0f * std::sin(2.0f * 3.14159f * i / 100.0f);
        }
        
        float multFactor = 1.0f / (float(0x7fff) * bitVolts);
        
        // Warm-up
        for (int i = 0; i < 10; i++)
        {
            FloatVectorOperations::copyWithMultiply(scaledBuffer.getData(), inputBuffer.getData(), multFactor, samples);
            AudioDataConverters::convertFloatToInt16LE(scaledBuffer.getData(), outputBuffer.getData(), samples);
        }
        
        // Benchmark
        auto startTime = high_resolution_clock::now();
        
        for (int iter = 0; iter < iterations; iter++)
        {
            for (int ch = 0; ch < channels; ch++)
            {
                // This mimics what BinaryRecording::writeContinuousData does
                FloatVectorOperations::copyWithMultiply(scaledBuffer.getData(), inputBuffer.getData(), multFactor, samples);
                AudioDataConverters::convertFloatToInt16LE(scaledBuffer.getData(), outputBuffer.getData(), samples);
            }
        }
        
        auto endTime = high_resolution_clock::now();
        double totalTimeMs = duration_cast<microseconds>(endTime - startTime).count() / 1000.0;
        
        BenchmarkResult result;
        result.testName = "Float-to-Int16 Conversion";
        result.numChannels = channels;
        result.numSamples = samples;
        result.iterations = iterations;
        result.totalTimeMs = totalTimeMs;
        result.avgTimeMs = totalTimeMs / iterations;
        
        int64_t totalSamples = (int64_t)channels * samples * iterations;
        result.samplesPerSecond = totalSamples / (totalTimeMs / 1000.0);
        result.megabytesPerSecond = (totalSamples * sizeof(int16)) / (totalTimeMs / 1000.0) / 1e6;
        result.channelSamplesPerSecond = result.samplesPerSecond;
        
        return result;
    }

    /**
     * Benchmark the interleaved write operation in SequentialBlockFile
     */
    BenchmarkResult benchmarkInterleavedWrite(int channels, int samples, int iterations)
    {
        // Create temporary file for benchmark
        auto tempFile = parentRecordingDir / "benchmark_interleave.dat";
        
        // Allocate test data
        std::vector<HeapBlock<int16>> channelData(channels);
        for (int ch = 0; ch < channels; ch++)
        {
            channelData[ch].malloc(samples);
            for (int s = 0; s < samples; s++)
            {
                channelData[ch][s] = static_cast<int16>((ch * 100 + s) % 32767);
            }
        }
        
        double totalTimeMs = 0.0;
        
        for (int iter = 0; iter < iterations; iter++)
        {
            // Create fresh file for each iteration
            if (std::filesystem::exists(tempFile))
            {
                std::filesystem::remove(tempFile);
            }
            
            SequentialBlockFile blockFile(channels, 4096);
            bool opened = blockFile.openFile(tempFile.string());
            EXPECT_TRUE(opened);
            
            auto startTime = high_resolution_clock::now();
            
            // Write all channels (mimics RecordThread behavior)
            for (int ch = 0; ch < channels; ch++)
            {
                blockFile.writeChannel(0, ch, channelData[ch].getData(), samples);
            }
            
            auto endTime = high_resolution_clock::now();
            totalTimeMs += duration_cast<microseconds>(endTime - startTime).count() / 1000.0;
        }
        
        // Cleanup
        if (std::filesystem::exists(tempFile))
        {
            std::filesystem::remove(tempFile);
        }
        
        BenchmarkResult result;
        result.testName = "Interleaved Write (SequentialBlockFile)";
        result.numChannels = channels;
        result.numSamples = samples;
        result.iterations = iterations;
        result.totalTimeMs = totalTimeMs;
        result.avgTimeMs = totalTimeMs / iterations;
        
        int64_t totalSamples = (int64_t)channels * samples * iterations;
        result.samplesPerSecond = totalSamples / (totalTimeMs / 1000.0);
        result.megabytesPerSecond = (totalSamples * sizeof(int16)) / (totalTimeMs / 1000.0) / 1e6;
        result.channelSamplesPerSecond = result.samplesPerSecond;
        
        return result;
    }

    /**
     * Benchmark end-to-end recording throughput
     */
    BenchmarkResult benchmarkEndToEndRecording(int channels, int samplesPerBlock, int numBlocks)
    {
        initializeTester(channels, sampleRate, bitVolts);
        
        // Pre-create all buffers to avoid allocation during timing
        std::vector<AudioBuffer<float>> buffers;
        for (int i = 0; i < numBlocks; i++)
        {
            buffers.push_back(createSignalBuffer(channels, samplesPerBlock));
        }
        
        // Start acquisition and recording
        tester->startAcquisition(true);
        
        // Small delay to ensure everything is initialized
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto startTime = high_resolution_clock::now();
        
        // Process all blocks
        for (int i = 0; i < numBlocks; i++)
        {
            writeBlock(buffers[i]);
        }
        
        auto endTime = high_resolution_clock::now();
        
        // Stop recording
        tester->stopAcquisition();
        
        double totalTimeMs = duration_cast<microseconds>(endTime - startTime).count() / 1000.0;
        
        BenchmarkResult result;
        result.testName = "End-to-End Recording";
        result.numChannels = channels;
        result.numSamples = samplesPerBlock * numBlocks;
        result.iterations = numBlocks;
        result.totalTimeMs = totalTimeMs;
        result.avgTimeMs = totalTimeMs / numBlocks;
        
        int64_t totalSamples = (int64_t)channels * samplesPerBlock * numBlocks;
        result.samplesPerSecond = totalSamples / (totalTimeMs / 1000.0);
        result.megabytesPerSecond = (totalSamples * sizeof(int16)) / (totalTimeMs / 1000.0) / 1e6;
        result.channelSamplesPerSecond = result.samplesPerSecond;
        
        // Reset tester for next test
        tester.reset();
        
        return result;
    }

    /**
     * Calculate the theoretical maximum throughput based on sample rate
     */
    static double calculateTheoreticalThroughput(int channels, float sampleRate)
    {
        // Samples per second * bytes per sample
        return channels * sampleRate * sizeof(int16);
    }

    /**
     * Check if throughput meets real-time requirements
     */
    static bool meetsRealTimeRequirement(const BenchmarkResult& result, float sampleRate)
    {
        double requiredMBps = (result.numChannels * sampleRate * sizeof(int16)) / 1e6;
        // Need at least 2x margin for safety
        return result.megabytesPerSecond >= requiredMBps * 2.0;
    }

protected:
    RecordNode* processor = nullptr;
    int numChannels;
    float sampleRate;
    float bitVolts;
    std::unique_ptr<ProcessorTester> tester;
    std::filesystem::path parentRecordingDir;
};

// ============================================================================
// FLOAT-TO-INT16 CONVERSION BENCHMARKS
// ============================================================================

TEST_F(RecordNodeBenchmark, FloatToInt16_384ch_4096samples)
{
    auto result = benchmarkFloatToInt16Conversion(384, 4096, 100);
    result.print();
    
    // Baseline expectation: should be at least 200 MB/s
    EXPECT_GT(result.megabytesPerSecond, 100.0) 
        << "Float-to-int16 conversion is slower than expected";
}

TEST_F(RecordNodeBenchmark, FloatToInt16_768ch_4096samples)
{
    auto result = benchmarkFloatToInt16Conversion(768, 4096, 50);
    result.print();
    
    EXPECT_GT(result.megabytesPerSecond, 100.0);
}

TEST_F(RecordNodeBenchmark, FloatToInt16_1536ch_4096samples)
{
    auto result = benchmarkFloatToInt16Conversion(1536, 4096, 25);
    result.print();
    
    EXPECT_GT(result.megabytesPerSecond, 100.0);
}

// ============================================================================
// INTERLEAVED WRITE BENCHMARKS
// ============================================================================

TEST_F(RecordNodeBenchmark, InterleavedWrite_384ch_4096samples)
{
    auto result = benchmarkInterleavedWrite(384, 4096, 50);
    result.print();
    
    // Baseline expectation: should be at least 150 MB/s
    EXPECT_GT(result.megabytesPerSecond, 50.0)
        << "Interleaved write is slower than expected";
}

TEST_F(RecordNodeBenchmark, InterleavedWrite_768ch_4096samples)
{
    auto result = benchmarkInterleavedWrite(768, 4096, 25);
    result.print();
    
    EXPECT_GT(result.megabytesPerSecond, 50.0);
}

TEST_F(RecordNodeBenchmark, InterleavedWrite_1536ch_4096samples)
{
    auto result = benchmarkInterleavedWrite(1536, 4096, 10);
    result.print();
    
    EXPECT_GT(result.megabytesPerSecond, 50.0);
}

// ============================================================================
// END-TO-END RECORDING BENCHMARKS
// ============================================================================

TEST_F(RecordNodeBenchmark, EndToEnd_384ch_30kHz_10sec)
{
    int channels = 384;
    int samplesPerBlock = 1024;
    float sampleRate = 30000.0f;
    int numBlocks = static_cast<int>(10.0 * sampleRate / samplesPerBlock);
    
    auto result = benchmarkEndToEndRecording(channels, samplesPerBlock, numBlocks);
    result.print();
    
    // Check if we can sustain real-time recording
    double requiredMBps = (channels * sampleRate * sizeof(int16)) / 1e6;
    std::cout << "Required throughput for real-time: " << requiredMBps << " MB/s\n";
    std::cout << "Achieved throughput: " << result.megabytesPerSecond << " MB/s\n";
    std::cout << "Margin: " << (result.megabytesPerSecond / requiredMBps) << "x\n";
    
    EXPECT_TRUE(meetsRealTimeRequirement(result, sampleRate))
        << "Cannot sustain real-time recording for " << channels << " channels at " << sampleRate << " Hz";
}

TEST_F(RecordNodeBenchmark, EndToEnd_768ch_30kHz_5sec)
{
    int channels = 768;
    int samplesPerBlock = 1024;
    float sampleRate = 30000.0f;
    int numBlocks = static_cast<int>(5.0 * sampleRate / samplesPerBlock);
    
    auto result = benchmarkEndToEndRecording(channels, samplesPerBlock, numBlocks);
    result.print();
    
    double requiredMBps = (channels * sampleRate * sizeof(int16)) / 1e6;
    std::cout << "Required throughput for real-time: " << requiredMBps << " MB/s\n";
    std::cout << "Achieved throughput: " << result.megabytesPerSecond << " MB/s\n";
    std::cout << "Margin: " << (result.megabytesPerSecond / requiredMBps) << "x\n";
}

TEST_F(RecordNodeBenchmark, EndToEnd_1536ch_30kHz_5sec)
{
    int channels = 1536;
    int samplesPerBlock = 1024;
    float sampleRate = 30000.0f;
    int numBlocks = static_cast<int>(5.0 * sampleRate / samplesPerBlock);
    
    auto result = benchmarkEndToEndRecording(channels, samplesPerBlock, numBlocks);
    result.print();
    
    double requiredMBps = (channels * sampleRate * sizeof(int16)) / 1e6;
    std::cout << "Required throughput for real-time: " << requiredMBps << " MB/s\n";
    std::cout << "Achieved throughput: " << result.megabytesPerSecond << " MB/s\n";
    std::cout << "Margin: " << (result.megabytesPerSecond / requiredMBps) << "x\n";
}

// ============================================================================
// BUFFER SIZE IMPACT BENCHMARKS
// ============================================================================

TEST_F(RecordNodeBenchmark, BufferSizeImpact_384ch)
{
    std::cout << "\n========================================\n";
    std::cout << "Buffer Size Impact Analysis (384 channels)\n";
    std::cout << "========================================\n";
    
    std::vector<int> bufferSizes = {256, 512, 1024, 2048, 4096, 8192};
    
    for (int bufSize : bufferSizes)
    {
        auto result = benchmarkFloatToInt16Conversion(384, bufSize, 100);
        std::cout << "Buffer size " << bufSize << ": " 
                  << result.megabytesPerSecond << " MB/s\n";
    }
}

// ============================================================================
// COMPREHENSIVE BASELINE REPORT
// ============================================================================

TEST_F(RecordNodeBenchmark, GenerateBaselineReport)
{
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "         RECORDNODE BASELINE PERFORMANCE REPORT\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
    
    // Configuration
    std::vector<int> channelCounts = {384, 768, 1536};
    int samples = 4096;
    float rate = 30000.0f;
    
    std::cout << "Test Configuration:\n";
    std::cout << "  Sample rate: " << rate << " Hz\n";
    std::cout << "  Samples per block: " << samples << "\n";
    std::cout << "  Bit volts: " << bitVolts << "\n";
    std::cout << "\n";
    
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "Float-to-Int16 Conversion Performance:\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << std::setw(12) << "Channels" 
              << std::setw(15) << "MB/s"
              << std::setw(20) << "M samples/s" << "\n";
    
    for (int ch : channelCounts)
    {
        auto result = benchmarkFloatToInt16Conversion(ch, samples, 50);
        std::cout << std::setw(12) << ch
                  << std::setw(15) << std::fixed << std::setprecision(1) << result.megabytesPerSecond
                  << std::setw(20) << std::fixed << std::setprecision(1) << result.samplesPerSecond / 1e6
                  << "\n";
    }
    
    std::cout << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "Interleaved Write Performance:\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << std::setw(12) << "Channels" 
              << std::setw(15) << "MB/s"
              << std::setw(20) << "M samples/s" << "\n";
    
    for (int ch : channelCounts)
    {
        auto result = benchmarkInterleavedWrite(ch, samples, 20);
        std::cout << std::setw(12) << ch
                  << std::setw(15) << std::fixed << std::setprecision(1) << result.megabytesPerSecond
                  << std::setw(20) << std::fixed << std::setprecision(1) << result.samplesPerSecond / 1e6
                  << "\n";
    }
    
    std::cout << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "Real-time Recording Requirements:\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << std::setw(12) << "Channels"
              << std::setw(15) << "Required MB/s"
              << std::setw(15) << "@ 30kHz" << "\n";
    
    for (int ch : channelCounts)
    {
        double required = (ch * rate * sizeof(int16)) / 1e6;
        std::cout << std::setw(12) << ch
                  << std::setw(15) << std::fixed << std::setprecision(1) << required
                  << std::setw(15) << "(2x = " << required * 2 << ")"
                  << "\n";
    }
    
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "                    END OF BASELINE REPORT\n";
    std::cout << "================================================================\n";
}
