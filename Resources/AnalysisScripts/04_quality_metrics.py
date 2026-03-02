"""
Compute Quality Metrics for Spike Sorting Output

Calculates:
  - ISI violations (contamination rate)
  - Signal-to-noise ratio
  - Firing rate
  - Amplitude cutoff
  - Presence ratio
  - Silhouette score (if PCA available)

Edit paths below.
"""

import spikeinterface.extractors as se
import spikeinterface.core as sc
from pathlib import Path

# ============================================================
# EDIT THESE
RECORDING_PATH = r"C:\data\experiment1\preprocessed"
SORTING_PATH = r"C:\data\experiment1\sorting"
ANALYZER_PATH = r"C:\data\experiment1\analyzer"
# ============================================================


def main():
    print("Loading recording and sorting...")
    recording = se.read_binary_folder(RECORDING_PATH)
    sorting = se.read_sorter_folder(SORTING_PATH)

    print(
        f"  {recording.get_num_channels()} channels, "
        f"{len(sorting.get_unit_ids())} units"
    )

    # Create SortingAnalyzer (replaces WaveformExtractor in SI >= 0.101)
    print("\nCreating SortingAnalyzer (this may take a few minutes)...")
    analyzer = sc.create_sorting_analyzer(
        sorting=sorting,
        recording=recording,
        format="binary_folder",
        folder=ANALYZER_PATH,
        sparse=True,
        overwrite=True,
    )

    # Compute required extensions
    print("Computing waveforms...")
    analyzer.compute("random_spikes")
    analyzer.compute("waveforms")
    analyzer.compute("templates")
    analyzer.compute("noise_levels")

    print("Computing quality metrics...")
    analyzer.compute("unit_locations")
    analyzer.compute("spike_amplitudes")
    analyzer.compute("correlograms")
    analyzer.compute("template_similarity")
    metrics = analyzer.compute("quality_metrics")

    # Display metrics
    df = metrics.get_data()
    print("\nQuality Metrics:")
    print(df.to_string())

    # Save to CSV
    csv_path = Path(ANALYZER_PATH) / "quality_metrics.csv"
    df.to_csv(csv_path)
    print(f"\nSaved metrics to: {csv_path}")

    # Summary statistics
    print("\n--- Summary ---")
    if "isi_violations_ratio" in df.columns:
        good_units = (df["isi_violations_ratio"] < 0.01).sum()
        print(f"Units with <1% ISI violations: {good_units}/{len(df)}")
    if "snr" in df.columns:
        high_snr = (df["snr"] > 5).sum()
        print(f"Units with SNR > 5: {high_snr}/{len(df)}")
    if "firing_rate" in df.columns:
        print(
            f"Firing rate range: {df['firing_rate'].min():.1f} - "
            f"{df['firing_rate'].max():.1f} Hz"
        )


if __name__ == "__main__":
    main()
