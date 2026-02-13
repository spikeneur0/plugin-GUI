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

#ifndef __LFPVIEWERPROCESSING_H__
#define __LFPVIEWERPROCESSING_H__

#include <ProcessorHeaders.h>
#include <vector>

namespace LfpViewer
{

/**
    Provides optional high-pass filtering and common average referencing (CAR)
    for the LFP Viewer, applied when copying data from the DisplayBuffer
    to the ScreenBuffer.

    Each LfpDisplaySplitter owns one instance, so processing is
    configurable per split view.

    When a Neuropixels probe is detected (via neuropixels.adcs metadata),
    the CAR is automatically switched to the Neuropixels-optimized
    ADC-group-based referencing scheme.
*/
class LfpViewerProcessing
{
public:
    /** Constructor */
    LfpViewerProcessing();

    /** Destructor */
    ~LfpViewerProcessing();

    /** Configure for a given channel count and sample rate.
        Also accepts channel types so that filtering/CAR can be
        restricted to ELECTRODE channels only. */
    void prepare (int numChannels, float sampleRate,
                  const Array<ContinuousChannel::Type>& channelTypes);

    /** Reset all filter states (e.g. on stream change) */
    void reset();

    /** Returns true if any processing is active */
    bool isActive() const { return highPassEnabled || carEnabled; }

    // ----- High-pass filter -----

    /** Enable / disable the high-pass filter */
    void setHighPassEnabled (bool enabled);

    /** Returns true if high-pass filter is enabled */
    bool isHighPassEnabled() const { return highPassEnabled; }

    /** Apply high-pass to multiple channels in one pass (SIMD-optimized path when available) */
    void applyHighPass (AudioBuffer<float>& buffer, int numSamples, int numChannels);

    // ----- Common Average Reference -----

    /** Enable / disable CAR */
    void setCAREnabled (bool enabled);

    /** Returns true if CAR is enabled */
    bool isCAREnabled() const { return carEnabled; }

    /** Set the Neuropixels ADC count (0 = standard full-channel CAR).
        When > 0, the Neuropixels ADC-group CAR is used instead. */
    void setNeuropixelsAdcCount (int adcCount);

    /** Returns true if Neuropixels-optimized CAR is active */
    bool isNeuropixelsMode() const { return numAdcs > 0; }

    /** Returns the number of ADCs (0 if not in NP mode) */
    int getNumAdcs() const { return numAdcs; }

    /** Apply CAR to a contiguous AudioBuffer of shape (numChannels, numSamples).
        Only ELECTRODE channels participate in the reference computation. */
    void applyCAR (AudioBuffer<float>& buffer, int numSamples, int numChannels);

private:
    bool highPassEnabled = false;
    float sampleRate = 30000.0f;
    int numChannels = 0;

    bool carEnabled = false;
    int numAdcs = 0;

    /** Shared biquad coefficients for the high-pass filter */
    float highPassB0 = 1.0f;
    float highPassB1 = 0.0f;
    float highPassB2 = 0.0f;
    float highPassA1 = 0.0f;
    float highPassA2 = 0.0f;

    /** Per-channel transposed-direct-form II states */
    std::vector<float> highPassZ1;
    std::vector<float> highPassZ2;

    /** Tracks which channels are ELECTRODE type */
    Array<bool> isElectrodeChannel;

    /** Buffer for the CAR average.
        Standard CAR: 1 channel.
        Neuropixels CAR: numGroups channels. */
    AudioBuffer<float> carAvgBuffer;

    /** Neuropixels channel-to-group mapping */
    Array<int> channelGroups;

    /** Number of active channels per group (for averaging) */
    Array<float> channelCounts;

    /** Number of ADC groups */
    int numGroups = 0;

    /** Creates / resets the high-pass filters */
    void createHighPassFilters();

    /** Updates biquad coefficients for the configured sample rate */
    void updateHighPassCoefficients();

    /** Initializes Neuropixels channel grouping based on ADC count */
    void setupNeuropixelsGroups();

    /** Standard full-channel CAR */
    void applyStandardCAR (AudioBuffer<float>& buffer, int numSamples, int numChannels);

    /** Neuropixels ADC-group CAR */
    void applyNeuropixelsCAR (AudioBuffer<float>& buffer, int numSamples, int numChannels);
};

} // namespace LfpViewer

#endif // __LFPVIEWERPROCESSING_H__
