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

#ifndef SEQUENTIALBLOCKFILE_H
#define SEQUENTIALBLOCKFILE_H

#include "../../../Utils/Utils.h"
#include "FileMemoryBlock.h"
#include "SIMDConverter.h"

#include "../../PluginManager/PluginClass.h"

typedef FileMemoryBlock<int16> FileBlock;

/**
 
    Writes data to a flat binary file of int16s
 
    Often referred to as "dat" or "bin" format, these files contain data
    samples in the following order with N channels and M samples per channel:
 
    <Channel 1 Sample 1>
    <Channel 1 Sample 2>
    ...
    <Channel 1 Sample M>
    ...
    ...
    <Channel N Sample 1>
    <Channel N Sample 2>
    ...
    <Channel N Sample M>

 */

class PLUGIN_API SequentialBlockFile
{
public:
    /** Creates a file with nChannels */
    SequentialBlockFile (int nChannels, int samplesPerBlock = 4096);

    /** Destructor */
    ~SequentialBlockFile();

    /** Opens the file at the requested path */
    bool openFile (String filename);

    /** Writes nSamples of data for a particular channel */
    bool writeChannel (uint64 startPos, int channel, int16* data, int nSamples);

    /** 
     * Writes data for all channels at once with optimized interleaving.
     * This is more efficient than calling writeChannel() for each channel separately
     * because it performs interleaving in a cache-friendly manner.
     * 
     * @param startPos     Starting sample position in the file
     * @param channelData  Array of pointers to int16 data for each channel
     * @param numChannels  Number of channels (must match m_nChannels)
     * @param nSamples     Number of samples per channel
     * @return true if write was successful
     */
    bool writeChannelBatch (uint64 startPos, int16* const* channelData, int numChannels, int nSamples);

private:
    std::shared_ptr<FileOutputStream> m_file;
    const int m_nChannels;
    const int m_samplesPerBlock;
    const int m_blockSize;
    OwnedArray<FileBlock> m_memBlocks;
    Array<int> m_currentBlock;
    size_t m_lastBlockFill;

    /** Allocates data for a startIndex / numSamples combination */
    void allocateBlocks (uint64 startIndex, int numSamples);

    /** Compile-time params */
    const int streamBufferSize { 65536 };  // 64KB buffer to reduce system calls
    const int blockArrayInitSize { 128 };

    /** Pre-allocated buffers for batch writing to avoid hot-path allocations */
    mutable std::vector<const int16_t*> m_batchChannelPtrs;
    SIMDConverter::TileConfig m_tileConfig;  // Cached tile config for this channel count
};
#endif // !SEQUENTIALBLOCKFILE_H
