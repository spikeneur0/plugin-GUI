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

#include "RecordThread.h"
#include "RecordNode.h"

//#define EVERY_ENGINE for(int eng = 0; eng < m_engineArray.size(); eng++) m_engineArray[eng]
#define EVERY_ENGINE m_engine;

RecordThread::RecordThread (RecordNode* parentNode, RecordEngine* engine) : Thread ("Record Thread"),
                                                                            m_engine (engine),
                                                                            recordNode (parentNode),
                                                                            m_receivedFirstBlock (false),
                                                                            m_cleanExit (true)
//samplesWritten(0)
{
}

RecordThread::~RecordThread()
{
}

void RecordThread::setEngine (RecordEngine* engine)
{
    m_engine = engine;
}

void RecordThread::setFileComponents (File rootFolder, int experimentNumber, int recordingNumber)
{
    if (isThreadRunning())
    {
        LOGD (__FUNCTION__, " Tried to set file components while thread was running!");
        return;
    }

    m_rootFolder = rootFolder;
    LOGD ("RecordThread::setFileComponents - Experiment number: ", experimentNumber, " Recording number: ", recordingNumber);
    m_experimentNumber = experimentNumber;
    m_recordingNumber = recordingNumber;
}

void RecordThread::setTimestampChannelMap (const Array<int>& channels)
{
    if (isThreadRunning())
        return;
    m_timestampBufferChannelArray = channels;
}

void RecordThread::setChannelMap (const Array<int>& channels)
{
    if (isThreadRunning())
        return;
    m_channelArray = channels;
    m_numChannels = channels.size();
}

void RecordThread::setQueuePointers (DataQueue* data, EventMsgQueue* events, SpikeMsgQueue* spikes)
{
    m_dataQueue = data;
    m_eventQueue = events;
    m_spikeQueue = spikes;
}

void RecordThread::setFirstBlockFlag (bool state)
{
    m_receivedFirstBlock = state;
    this->notify();
}

void RecordThread::run()
{
    const AudioBuffer<float>& dataBuffer = m_dataQueue->getContinuousDataBufferReference();
    const SynchronizedTimestampBuffer& ftsBuffer = m_dataQueue->getTimestampBufferReference();

    spikesReceived = 0;
    spikesWritten = 0;

    sampleNumbers.clear();
    dataBufferIdxs.clear();
    timestampBufferIdxs.clear();

    for (int chan = 0; chan < m_numChannels; ++chan)
    {
        sampleNumbers.add (0);
        dataBufferIdxs.push_back (CircularBufferIndexes());
    }

    for (int stream = 0; stream < recordNode->getNumDataStreams(); ++stream)
    {
        timestampBufferIdxs.push_back (CircularBufferIndexes());
    }

    bool closeEarly = true;

    //1-Open Files
    m_cleanExit = false;
    closeEarly = false;
    Array<int64> sampleNumbers;

    m_engine->openFiles (m_rootFolder, m_experimentNumber, m_recordingNumber);

    //2-Wait until the first block has arrived, so we can align the timestamps
    bool isWaiting = false;
    while (! m_receivedFirstBlock && ! threadShouldExit())
    {
        if (! isWaiting)
        {
            isWaiting = true;
        }
        wait (1);
    }

    m_dataQueue->getSampleNumbersForBlock (0, sampleNumbers);
    m_engine->updateLatestSampleNumbers (sampleNumbers);

    //3-Normal loop
    while (! threadShouldExit())
        writeData (dataBuffer, ftsBuffer, BLOCK_MAX_WRITE_SAMPLES, BLOCK_MAX_WRITE_EVENTS, BLOCK_MAX_WRITE_SPIKES);

    //LOGD(__FUNCTION__, " Exiting record thread");
    //4-Before closing the thread, try to write the remaining samples

    LOGD ("Closing all files");

    if (! closeEarly)
    {
        // flush the buffers
        writeData (dataBuffer, ftsBuffer, BLOCK_MAX_WRITE_SAMPLES, BLOCK_MAX_WRITE_EVENTS, BLOCK_MAX_WRITE_SPIKES, true);

        //5-Close files
        m_engine->closeFiles();
    }
    m_cleanExit = true;
    m_receivedFirstBlock = false;

    //LOGC("RecordThread received ", spikesReceived, " spikes and wrote ", spikesWritten, ".");
}

void RecordThread::writeData (const AudioBuffer<float>& dataBuffer,
                              const SynchronizedTimestampBuffer& timestampBuffer,
                              int maxSamples,
                              int maxEvents,
                              int maxSpikes,
                              bool lastBlock)
{
    if (m_dataQueue->startRead (dataBufferIdxs, timestampBufferIdxs, sampleNumbers, maxSamples))
    {
        m_engine->updateLatestSampleNumbers (sampleNumbers);

        // Group channels by stream (identified by timestamp buffer channel) for batch writes
        // Channels with the same timestamp buffer belong to the same stream
        int chan = 0;
        while (chan < m_numChannels)
        {
            // Skip channels with no data
            if (dataBufferIdxs[chan].size1 == 0)
            {
                chan++;
                continue;
            }

            // Find all consecutive channels belonging to the same stream with matching buffer positions
            int currentStream = m_timestampBufferChannelArray[chan];
            int batchStartChan = chan;
            int numSamples = dataBufferIdxs[chan].size1;
            int bufferIndex = dataBufferIdxs[chan].index1;

            // Clear batch arrays
            m_batchWriteChannels.clear();
            m_batchRealChannels.clear();
            m_batchDataPtrs.clear();

            // Collect channels for this batch
            while (chan < m_numChannels &&
                   m_timestampBufferChannelArray[chan] == currentStream &&
                   dataBufferIdxs[chan].size1 == numSamples &&
                   dataBufferIdxs[chan].index1 == bufferIndex)
            {
                m_batchWriteChannels.push_back (chan);
                m_batchRealChannels.push_back (m_channelArray[chan]);
                m_batchDataPtrs.push_back (dataBuffer.getReadPointer (chan, bufferIndex));
                chan++;
            }

            int batchSize = static_cast<int> (m_batchWriteChannels.size());

            // Get timestamp buffer for this stream
            const double* timestamps = timestampBuffer.getReadPointer (currentStream, bufferIndex);

            // Use batch write if we have multiple channels, otherwise use single-channel write
            if (batchSize > 1)
            {
                m_engine->writeContinuousDataBatch (
                    m_batchWriteChannels.data(),
                    m_batchRealChannels.data(),
                    m_batchDataPtrs.data(),
                    timestamps,
                    batchSize,
                    numSamples,
                    currentStream); // file index = stream index for BinaryRecording
            }
            else
            {
                m_engine->writeContinuousData (
                    m_batchWriteChannels[0],
                    m_batchRealChannels[0],
                    m_batchDataPtrs[0],
                    timestamps,
                    numSamples);
            }

            // Handle wrap-around (size2) if present
            if (dataBufferIdxs[batchStartChan].size2 > 0)
            {
                int numSamples2 = dataBufferIdxs[batchStartChan].size2;
                int bufferIndex2 = dataBufferIdxs[batchStartChan].index2;

                // Update sample numbers for all channels in the batch
                for (int i = 0; i < batchSize; i++)
                {
                    int batchChan = m_batchWriteChannels[i];
                    sampleNumbers.set (batchChan, sampleNumbers[batchChan] + numSamples);
                }
                m_engine->updateLatestSampleNumbers (sampleNumbers, m_batchWriteChannels[0]);

                // Update data pointers for the second segment
                for (int i = 0; i < batchSize; i++)
                {
                    int batchChan = m_batchWriteChannels[i];
                    m_batchDataPtrs[i] = dataBuffer.getReadPointer (batchChan, bufferIndex2);
                }

                const double* timestamps2 = timestampBuffer.getReadPointer (currentStream, bufferIndex2);

                if (batchSize > 1)
                {
                    m_engine->writeContinuousDataBatch (
                        m_batchWriteChannels.data(),
                        m_batchRealChannels.data(),
                        m_batchDataPtrs.data(),
                        timestamps2,
                        batchSize,
                        numSamples2,
                        currentStream);
                }
                else
                {
                    m_engine->writeContinuousData (
                        m_batchWriteChannels[0],
                        m_batchRealChannels[0],
                        m_batchDataPtrs[0],
                        timestamps2,
                        numSamples2);
                }
            }
        }

        m_dataQueue->stopRead();
    }

    std::vector<EventMessagePtr> events;
    int nEvents = m_eventQueue->getEvents (events, maxEvents);

    for (int ev = 0; ev < nEvents; ++ev)
    {
        const MidiMessage& event = events[ev]->getData();

        if (SystemEvent::getBaseType (event) == EventBase::Type::SYSTEM_EVENT)
        {
            String syncText = SystemEvent::getSyncText (event);
            m_engine->writeTimestampSyncText (SystemEvent::getStreamId (event), SystemEvent::getSampleNumber (event), 0.0f, SystemEvent::getSyncText (event));
        }
        else
        {
            int processorId = EventBase::getProcessorId (event);
            int streamId = EventBase::getStreamId (event);
            int channelIdx = EventBase::getChannelIndex (event);

            const EventChannel* chan = recordNode->getEventChannel (processorId, streamId, channelIdx);
            int eventIndex = recordNode->getIndexOfMatchingChannel (chan);

            m_engine->writeEvent (eventIndex, event);
        }
    }

    std::vector<SpikeMessagePtr> spikes;
    int nSpikes = m_spikeQueue->getEvents (spikes, BLOCK_MAX_WRITE_SPIKES);

    for (int sp = 0; sp < nSpikes; ++sp)
    {
        spikesReceived++;

        if (spikes[sp] != nullptr)
        {
            const Spike& spike = spikes[sp]->getData();
            const SpikeChannel* chan = spike.getChannelInfo();
            int spikeIndex = recordNode->getIndexOfMatchingChannel (chan);
            spikesWritten++;

            m_engine->writeSpike (spikeIndex, &spikes[sp]->getData());
        }
    }
}

void RecordThread::forceCloseFiles()
{
    if (isThreadRunning() || m_cleanExit)
        return;

    m_engine->closeFiles();
    m_cleanExit = true;
}
