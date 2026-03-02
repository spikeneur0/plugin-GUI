"""
Preprocess a SNAP Recording

Applies standard preprocessing steps:
  1. Bandpass filter (300-6000 Hz for spikes, or 0.5-300 Hz for LFP)
  2. Common median reference (CMR) to remove common noise
  3. Optional: bad channel detection and interpolation

Edit RECORDING_PATH and parameters below.
"""

import spikeinterface.extractors as se
import spikeinterface.preprocessing as sp
import matplotlib.pyplot as plt
import numpy as np

# ============================================================
# EDIT THESE
RECORDING_PATH = r"C:\data\experiment1\Record Node 101"
OUTPUT_PATH = r"C:\data\experiment1\preprocessed"
FREQ_MIN = 300.0  # Hz -- lower cutoff (300 for spikes, 0.5 for LFP)
FREQ_MAX = 6000.0  # Hz -- upper cutoff (6000 for spikes, 300 for LFP)
# ============================================================


def main():
    print(f"Loading recording from: {RECORDING_PATH}")
    recording = se.read_openephys(RECORDING_PATH, stream_id="0")
    print(
        f"  {recording.get_num_channels()} channels, "
        f"{recording.get_sampling_frequency()} Hz, "
        f"{recording.get_total_duration():.1f}s"
    )

    # Step 1: Bandpass filter
    print(f"\nApplying bandpass filter ({FREQ_MIN}-{FREQ_MAX} Hz)...")
    recording_filtered = sp.bandpass_filter(
        recording, freq_min=FREQ_MIN, freq_max=FREQ_MAX, margin_ms=5.0
    )

    # Step 2: Common median reference
    print("Applying common median reference...")
    recording_cmr = sp.common_reference(
        recording_filtered, reference="global", operator="median"
    )

    # Step 3: Detect bad channels (optional)
    print("Detecting bad channels...")
    bad_channel_ids, channel_labels = sp.detect_bad_channels(
        recording_cmr, method="coherence+psd", seed=42
    )
    if len(bad_channel_ids) > 0:
        print(f"  Found {len(bad_channel_ids)} bad channels: {bad_channel_ids}")
        recording_clean = recording_cmr.remove_channels(bad_channel_ids)
    else:
        print("  No bad channels detected")
        recording_clean = recording_cmr

    # Save preprocessed recording
    print(f"\nSaving preprocessed recording to: {OUTPUT_PATH}")
    recording_clean.save(
        folder=OUTPUT_PATH, format="binary", n_jobs=4, chunk_duration="1s"
    )

    # Plot comparison: raw vs preprocessed
    print("Plotting comparison...")
    fig, axes = plt.subplots(2, 1, figsize=(14, 8), sharex=True)

    n_channels = min(8, recording.get_num_channels())
    channel_ids = recording.get_channel_ids()[:n_channels]
    n_samples = int(0.5 * recording.get_sampling_frequency())  # 0.5 seconds

    raw = recording.get_traces(
        end_frame=n_samples, channel_ids=channel_ids, return_scaled=True
    )
    clean = recording_clean.get_traces(
        end_frame=n_samples,
        channel_ids=recording_clean.get_channel_ids()[:n_channels],
        return_scaled=True,
    )

    time = np.arange(n_samples) / recording.get_sampling_frequency()

    for i in range(n_channels):
        axes[0].plot(time, raw[:, i] - i * 200, "k", linewidth=0.3)
        axes[1].plot(time, clean[:, i] - i * 200, "k", linewidth=0.3)

    axes[0].set_title("Raw")
    axes[1].set_title(f"Preprocessed ({FREQ_MIN}-{FREQ_MAX} Hz + CMR)")
    axes[1].set_xlabel("Time (s)")

    plt.tight_layout()
    plt.savefig("02_preprocessing_comparison.png", dpi=150)
    print("Saved plot to 02_preprocessing_comparison.png")
    plt.show()


if __name__ == "__main__":
    main()
