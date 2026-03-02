"""
Run Spike Sorting on a SNAP Recording

Supports multiple sorters via SpikeInterface:
  - KiloSort4 (requires MATLAB + GPU)
  - MountainSort5 (pure Python, CPU-only)
  - SpyKING CIRCUS 2 (Python, CPU or GPU)

Edit RECORDING_PATH and SORTER below.
"""

import spikeinterface.extractors as se
import spikeinterface.sorters as ss
from pathlib import Path

# ============================================================
# EDIT THESE
RECORDING_PATH = r"C:\data\experiment1\preprocessed"
SORTING_OUTPUT = r"C:\data\experiment1\sorting"
SORTER = "mountainsort5"  # Options: "kilosort4", "mountainsort5", "spykingcircus2"
# ============================================================


def main():
    print(f"Loading preprocessed recording from: {RECORDING_PATH}")
    recording = se.read_binary_folder(RECORDING_PATH)
    print(
        f"  {recording.get_num_channels()} channels, "
        f"{recording.get_sampling_frequency()} Hz, "
        f"{recording.get_total_duration():.1f}s"
    )

    # Check available sorters
    available = ss.available_sorters()
    print(f"\nAvailable sorters: {available}")

    if SORTER not in available:
        print(f"\n{SORTER} is not installed. Install it with:")
        if SORTER == "kilosort4":
            print("  pip install kilosort")
        elif SORTER == "mountainsort5":
            print("  pip install mountainsort5")
        elif SORTER == "spykingcircus2":
            print("  pip install spykingcircus2")
        return

    # Run spike sorting
    print(f"\nRunning {SORTER}...")
    sorting = ss.run_sorter(
        sorter_name=SORTER,
        recording=recording,
        output_folder=SORTING_OUTPUT,
        remove_existing_folder=True,
        verbose=True,
    )

    print(f"\nSorting complete!")
    print(f"  Units found: {len(sorting.get_unit_ids())}")
    for unit_id in sorting.get_unit_ids():
        spike_train = sorting.get_unit_spike_train(unit_id)
        print(f"  Unit {unit_id}: {len(spike_train)} spikes")

    print(f"\nResults saved to: {SORTING_OUTPUT}")
    print("Run 04_quality_metrics.py next to assess sorting quality.")
    print("Run 05_export_to_phy.py to manually curate in Phy.")


if __name__ == "__main__":
    main()
