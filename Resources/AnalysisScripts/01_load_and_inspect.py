"""
Load and Inspect a SNAP Recording

This script loads a recording saved by SNAP (in Open Ephys binary format)
and displays basic information about the data.

Edit RECORDING_PATH below to point to your recording directory.
The path should be the Record Node directory containing 'settings.xml'.
"""

import spikeinterface.extractors as se
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# ============================================================
# EDIT THIS: Path to your SNAP recording directory
RECORDING_PATH = r"C:\data\experiment1\Record Node 101"
# ============================================================


def main():
    print(f"Loading recording from: {RECORDING_PATH}")

    # Load the recording using SpikeInterface's OpenEphys extractor
    recording = se.read_openephys(RECORDING_PATH, stream_id="0")

    # Print basic info
    print(f"\nRecording info:")
    print(f"  Channels: {recording.get_num_channels()}")
    print(f"  Sampling rate: {recording.get_sampling_frequency()} Hz")
    print(f"  Duration: {recording.get_total_duration():.1f} seconds")
    print(f"  Total samples: {recording.get_total_samples()}")

    # Print channel IDs
    channel_ids = recording.get_channel_ids()
    print(
        f"  Channel IDs: {channel_ids[:10]}{'...' if len(channel_ids) > 10 else ''}"
    )

    # Check if probe information is available
    if recording.has_probe():
        probe = recording.get_probe()
        print(f"\nProbe info:")
        print(f"  Name: {probe.annotations.get('name', 'Unknown')}")
        print(f"  Manufacturer: {probe.annotations.get('manufacturer', 'Unknown')}")
        print(f"  Contacts: {probe.get_contact_count()}")
        print(f"  Shanks: {len(probe.get_shanks())}")
    else:
        print("\nNo probe information attached to this recording.")
        print("You can load a ProbeInterface JSON file to attach probe geometry.")

    # Load a probe definition if available
    probe_file = Path(RECORDING_PATH) / "probe.json"
    if probe_file.exists():
        from probeinterface import read_probeinterface

        probegroup = read_probeinterface(str(probe_file))
        recording = recording.set_probegroup(probegroup)
        print(f"  Loaded probe from: {probe_file}")

    # Plot a short segment of raw data
    print("\nPlotting 1 second of raw traces...")
    fig, ax = plt.subplots(figsize=(14, 8))

    # Get 1 second of data from channels 0-15 (or all if fewer)
    n_channels = min(16, recording.get_num_channels())
    traces = recording.get_traces(
        start_frame=0,
        end_frame=int(recording.get_sampling_frequency()),
        channel_ids=channel_ids[:n_channels],
        return_scaled=True,  # Return in microvolts
    )

    # Plot with offset
    time = np.arange(traces.shape[0]) / recording.get_sampling_frequency()
    offset = 0
    for i in range(n_channels):
        ax.plot(time, traces[:, i] + offset, linewidth=0.5, color="black")
        ax.text(
            -0.01,
            offset,
            f"Ch {channel_ids[i]}",
            fontsize=8,
            ha="right",
            va="center",
            transform=ax.get_yaxis_transform(),
        )
        offset -= 200  # 200 uV spacing

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Channels")
    ax.set_title(f"SNAP Recording \u2014 First 1s, Channels 0-{n_channels - 1}")
    ax.set_yticks([])
    plt.tight_layout()
    plt.savefig("01_raw_traces.png", dpi=150)
    print("Saved plot to 01_raw_traces.png")
    plt.show()


if __name__ == "__main__":
    main()
