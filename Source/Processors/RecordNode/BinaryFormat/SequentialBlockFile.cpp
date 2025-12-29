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

#include "SequentialBlockFile.h"

SequentialBlockFile::SequentialBlockFile (int nChannels, int samplesPerBlock) : m_file (nullptr),
                                                                                m_nChannels (nChannels),
                                                                                m_samplesPerBlock (samplesPerBlock),
                                                                                m_blockSize (nChannels * samplesPerBlock),
                                                                                m_lastBlockFill (0)
{
    m_memBlocks.ensureStorageAllocated (blockArrayInitSize);
    for (int i = 0; i < nChannels; i++)
        m_currentBlock.add (-1);
}

SequentialBlockFile::~SequentialBlockFile()
{
    //Ensure that all remaining blocks are flushed in order. Keep the last one
    int n = m_memBlocks.size();
    for (int i = 0; i < n - 1; i++)
    {
        m_memBlocks.remove (0);
    }

    //manually flush the last one to avoid trailing zeroes
    if (m_memBlocks.size() > 0)
        m_memBlocks[0]->partialFlush (m_lastBlockFill * m_nChannels);
}

bool SequentialBlockFile::openFile (String filename)
{
    File file (filename);
    Result res = file.create();
    if (res.failed())
    {
        LOGD ("Error creating file ", filename, ": ", res.getErrorMessage());
        file.deleteFile();
        Result res = file.create();
        LOGD ("Re-creating file: ", filename);
    }

    m_file = file.createOutputStream (streamBufferSize);
    if (! m_file)
    {
        LOGD ("Unable to create output stream!");
        return false;
    }

    LOGDD ("Added new FileBlock");
    m_memBlocks.add (new FileBlock (m_file, m_blockSize, 0));
    return true;
}

bool SequentialBlockFile::writeChannel (uint64 startPos, int channel, int16* data, int nSamples)
{
    if (! m_file)
    {
        printf ("[RN]SequentialBlockFile::writeChannel returned false: (!m_file)\n");
        return false;
    }

    int bIndex = m_memBlocks.size() - 1;
    if ((bIndex < 0) || (m_memBlocks[bIndex]->getOffset() + m_samplesPerBlock) < (startPos + nSamples))
        allocateBlocks (startPos, nSamples);

    for (bIndex = m_memBlocks.size() - 1; bIndex >= 0; bIndex--)
    {
        if (m_memBlocks[bIndex]->getOffset() <= startPos)
            break;
    }
    if (bIndex < 0)
    {
        //LOGE("Memory block unloaded ahead of time for chan", channel, " start ", startPos, " ns ", nSamples);
        //for (int i = 0; i < m_nChannels; i++)
        //	LOGE("CH: ", i, " last block ", m_currentBlock[i]);
        return false;
    }
    int writtenSamples = 0;
    uint64 startIdx = startPos - m_memBlocks[bIndex]->getOffset();
    uint64 startMemPos = startIdx * m_nChannels;
    int dataIdx = 0;
    int lastBlockIdx = m_memBlocks.size() - 1;

    while (writtenSamples < nSamples)
    {
        int16* blockPtr = m_memBlocks[bIndex]->getData();
        int samplesToWrite = jmin ((nSamples - writtenSamples), (m_samplesPerBlock - int (startIdx)));

        for (int i = 0; i < samplesToWrite; i++)
        {
            *(blockPtr + startMemPos + channel + i * m_nChannels) = *(data + dataIdx);
            dataIdx++;
        }
        writtenSamples += samplesToWrite;

        //Update the last block fill index
        size_t samplePos = startIdx + samplesToWrite;
        if (bIndex == lastBlockIdx && samplePos > m_lastBlockFill)
        {
            m_lastBlockFill = samplePos;
        }

        startIdx = 0;
        startMemPos = 0;
        bIndex++;
    }
    m_currentBlock.set (channel, bIndex - 1); //store the last block a channel was written in
    return true;
}

bool SequentialBlockFile::writeChannelBatch (uint64 startPos, int16* const* channelData, int numChannels, int nSamples)
{
    if (! m_file)
    {
        printf ("[RN]SequentialBlockFile::writeChannelBatch returned false: (!m_file)\n");
        return false;
    }

    if (numChannels != m_nChannels)
    {
        printf ("[RN]SequentialBlockFile::writeChannelBatch: channel count mismatch (%d vs %d)\n", 
                numChannels, m_nChannels);
        return false;
    }

    int bIndex = m_memBlocks.size() - 1;
    if ((bIndex < 0) || (m_memBlocks[bIndex]->getOffset() + m_samplesPerBlock) < (startPos + nSamples))
        allocateBlocks (startPos, nSamples);

    for (bIndex = m_memBlocks.size() - 1; bIndex >= 0; bIndex--)
    {
        if (m_memBlocks[bIndex]->getOffset() <= startPos)
            break;
    }
    if (bIndex < 0)
    {
        return false;
    }

    int writtenSamples = 0;
    uint64 startIdx = startPos - m_memBlocks[bIndex]->getOffset();
    int dataIdx = 0;
    int lastBlockIdx = m_memBlocks.size() - 1;

    // Process in blocks for better cache utilization
    // Block size chosen to fit in L1 cache (typically 32-64KB)
    const int cacheBlockSamples = 64;  // Process 64 samples at a time

    // Cache blocking parameters - chosen to fit in L1 cache (~32KB)
    // For a tile of TILE_SAMPLES x TILE_CHANNELS:
    // - Input: TILE_CHANNELS pointers (8 bytes each) + TILE_SAMPLES * TILE_CHANNELS * 2 bytes data
    // - Output: TILE_SAMPLES * nChannels * 2 bytes (but we only write TILE_CHANNELS at a time)
    // With 256 samples x 64 channels: 256 * 64 * 2 = 32KB input data per tile
    const int TILE_SAMPLES = 256;
    const int TILE_CHANNELS = 64;

    while (writtenSamples < nSamples)
    {
        int16* blockPtr = m_memBlocks[bIndex]->getData();
        int samplesToWrite = jmin ((nSamples - writtenSamples), (m_samplesPerBlock - int (startIdx)));

        uint64 baseMemPos = startIdx * m_nChannels;

        // Process in tiles to optimize cache usage
        // For each tile of samples:
        for (int sampleTileStart = 0; sampleTileStart < samplesToWrite; sampleTileStart += TILE_SAMPLES)
        {
            int sampleTileEnd = jmin (sampleTileStart + TILE_SAMPLES, samplesToWrite);

            // For each tile of channels:
            for (int channelTileStart = 0; channelTileStart < m_nChannels; channelTileStart += TILE_CHANNELS)
            {
                int channelTileEnd = jmin (channelTileStart + TILE_CHANNELS, m_nChannels);

                // Process this tile - iterate samples in outer loop to optimize output writes
                for (int s = sampleTileStart; s < sampleTileEnd; s++)
                {
                    uint64 memPos = baseMemPos + s * m_nChannels + channelTileStart;
                    int srcIdx = dataIdx + s;

                    // Write channels in this tile for this sample
                    for (int ch = channelTileStart; ch < channelTileEnd; ch++)
                    {
                        blockPtr[memPos + (ch - channelTileStart)] = channelData[ch][srcIdx];
                    }
                }
            }
        }

        writtenSamples += samplesToWrite;
        dataIdx += samplesToWrite;

        // Update the last block fill index
        size_t samplePos = startIdx + samplesToWrite;
        if (bIndex == lastBlockIdx && samplePos > m_lastBlockFill)
        {
            m_lastBlockFill = samplePos;
        }

        startIdx = 0;
        bIndex++;
    }

    // Update current block for all channels
    for (int ch = 0; ch < m_nChannels; ch++)
    {
        m_currentBlock.set (ch, bIndex - 1);
    }

    return true;
}

void SequentialBlockFile::allocateBlocks (uint64 startIndex, int numSamples)
{
    //First deallocate full blocks
    //Search for the earliest unused block;
    unsigned int minBlock = 0xFFFFFFFF; //large number;
    for (int i = 0; i < m_nChannels; i++)
    {
        if (m_currentBlock[i] < minBlock)
            minBlock = m_currentBlock[i];
    }

    //Update block indexes
    for (int i = 0; i < m_nChannels; i++)
    {
        m_currentBlock.set (i, m_currentBlock[i] - minBlock);
    }

    m_memBlocks.removeRange (0, minBlock);

    //Look for the last available position and calculate needed space
    uint64 lastOffset = m_memBlocks.getLast()->getOffset();
    uint64 maxAddr = lastOffset + m_samplesPerBlock - 1;
    uint64 newSpaceNeeded = numSamples - (maxAddr - startIndex);
    uint64 newBlocks = (newSpaceNeeded + m_samplesPerBlock - 1) / m_samplesPerBlock; //Fast ceiling division

    for (int i = 0; i < newBlocks; i++)
    {
        lastOffset += m_samplesPerBlock;
        m_memBlocks.add (new FileBlock (m_file, m_blockSize, lastOffset));
    }
    if (newBlocks > 0)
        m_lastBlockFill = 0; //we've added some new blocks, so the last one will be empty
}
