# End-to-End SpikeInterface Analysis Pipeline for SNAP Recordings

This guide walks through a complete spike sorting and quality analysis pipeline
for recordings acquired with SNAP (the Open Ephys GUI). It mirrors the five
analysis scripts shipped in `Resources/AnalysisScripts/` and uses the
**SpikeInterface >= 0.101** API throughout (SortingAnalyzer, `sc.load()`,
`ss.read_sorter_folder()`).

> **API note.** Older tutorials reference `read_binary_folder()` and
> `WaveformExtractor`. Those calls are **deprecated**. This guide uses only the
> current API surface.

---

## Prerequisites and Installation

### Python Environment

Python 3.9 or later is recommended. Create and activate a virtual environment,
then install the pinned dependencies:

```bash
python -m venv .venv
source .venv/bin/activate          # Linux / macOS
# .venv\Scripts\activate           # Windows

pip install -r Resources/AnalysisScripts/requirements.txt
```

### requirements.txt

```
spikeinterface[full]>=0.101.0
probeinterface>=0.2.23
matplotlib>=3.5.0
numpy>=1.20.0
```

The `[full]` extra pulls in every SpikeInterface submodule (extractors,
preprocessing, sorters, exporters, quality metrics, widgets) plus optional
heavy dependencies such as scikit-learn.

### External Sorters

SpikeInterface wraps external spike sorters. Install at least one before
running Step 3:

| Sorter          | Install                                        |
| --------------- | ---------------------------------------------- |
| Kilosort 4      | `pip install kilosort` (requires CUDA GPU)     |
| MountainSort 5  | `pip install mountainsort5`                    |
| SpykingCircus 2 | Included with `spikeinterface[full]`           |

### Phy (optional, for manual curation)

```bash
pip install phy
```

---

## Pipeline Overview

```
 SNAP Recording (Open Ephys format)
         |
         v
 +-------------------------------+
 | 1. Load & Inspect             |  01_load_and_inspect.py
 |    se.read_openephys()        |
 +-------------------------------+
         |
         v
 +-------------------------------+
 | 2. Preprocess                 |  02_preprocessing.py
 |    bandpass 300-6000 Hz       |
 |    common median reference    |
 |    bad channel removal        |
 +-------------------------------+
         |
         v
 +-------------------------------+
 | 3. Spike Sorting              |  03_spike_sorting.py
 |    ss.run_sorter()            |
 |    kilosort4 / mountainsort5  |
 |    / spykingcircus2           |
 +-------------------------------+
         |
         v
 +-------------------------------+
 | 4. Quality Metrics            |  04_quality_metrics.py
 |    SortingAnalyzer            |
 |    ISI violations, SNR,       |
 |    firing rate, etc.          |
 +-------------------------------+
         |
         v
 +-------------------------------+
 | 5. Export to Phy               |  05_export_to_phy.py
 |    export_to_phy()            |
 |    manual curation in Phy     |
 +-------------------------------+
```

---

## Step 1 -- Load and Inspect the Recording

**Script:** `01_load_and_inspect.py`

Point `RECORDING_PATH` at the **Record Node** directory that contains
`settings.xml` (for example,
`Record Node 101/experiment1/recording1`).

```python
from pathlib import Path
import matplotlib.pyplot as plt
import spikeinterface.extractors as se

# ---------- configuration ----------
RECORDING_PATH = Path("/data/snap/Record Node 101")
STREAM_ID = "0"
# -----------------------------------

# Load the Open Ephys recording
recording = se.read_openephys(RECORDING_PATH, stream_id=STREAM_ID)

# Basic properties
print(f"Number of channels : {recording.get_num_channels()}")
print(f"Sampling frequency : {recording.get_sampling_frequency()} Hz")
print(f"Total duration     : {recording.get_total_duration():.2f} s")
print(f"Total samples      : {recording.get_total_samples()}")
print(f"Channel IDs        : {recording.get_channel_ids()}")

# Probe geometry (if saved with recording)
if recording.has_probe():
    probe = recording.get_probe()
    print(f"Probe              : {probe}")
else:
    print("No probe attached -- load one with probeinterface.read_probeinterface()")

# Plot 1 second of raw traces
traces = recording.get_traces(start_frame=0,
                              end_frame=int(recording.get_sampling_frequency()))
fig, ax = plt.subplots(figsize=(12, 6))
ax.plot(traces[:, :8])          # first 8 channels
ax.set_xlabel("Sample")
ax.set_ylabel("Amplitude (uV)")
ax.set_title("Raw traces -- first 1 s, first 8 channels")
plt.tight_layout()
plt.show()
```

### What to look for

* **Sampling frequency** should match your SNAP acquisition settings (typically
  30 000 Hz).
* **Channel count** should match your probe.
* Large DC offsets or obviously dead channels will be handled in Step 2.

---

## Step 2 -- Preprocessing

**Script:** `02_preprocessing.py`

SpikeInterface preprocessing operations are *lazy* -- they record a chain of
transformations but do not materialise data until you call `.save()` or a
downstream step pulls traces.

```python
from pathlib import Path
import spikeinterface.extractors as se
import spikeinterface.preprocessing as sp

# ---------- configuration ----------
RECORDING_PATH = Path("/data/snap/Record Node 101")
STREAM_ID = "0"
OUTPUT_PATH = Path("/data/snap/preprocessed")
# -----------------------------------

# 1. Load raw recording
recording = se.read_openephys(RECORDING_PATH, stream_id=STREAM_ID)

# 2. Bandpass filter (300 -- 6000 Hz)
recording_filtered = sp.bandpass_filter(
    recording,
    freq_min=300.0,
    freq_max=6000.0,
    margin_ms=5.0,
)

# 3. Common MEDIAN reference (global)
recording_cmr = sp.common_reference(
    recording_filtered,
    reference="global",
    operator="median",          # median, NOT average
)

# 4. Detect and remove bad channels
bad_channel_ids, bad_labels = sp.detect_bad_channels(
    recording_cmr,
    method="coherence+psd",
    seed=42,
)
print(f"Bad channels detected: {bad_channel_ids}")
recording_clean = recording_cmr.remove_channels(bad_channel_ids)

# 5. Save to disk (binary format, parallelised)
recording_clean.save(
    folder=OUTPUT_PATH,
    format="binary",
    n_jobs=4,
    chunk_duration="1s",
)
print(f"Preprocessed recording saved to {OUTPUT_PATH}")
```

### Key decisions

| Parameter              | Value        | Rationale                                      |
| ---------------------- | ------------ | ---------------------------------------------- |
| `freq_min`             | 300 Hz       | Removes LFP and movement artefacts             |
| `freq_max`             | 6000 Hz      | Keeps the spike band while rejecting noise     |
| `operator`             | `"median"`   | More robust to outlier channels than `"average"`|
| `method` (bad ch.)     | `"coherence+psd"` | Combines two complementary heuristics    |

---

## Step 3 -- Spike Sorting

**Script:** `03_spike_sorting.py`

```python
from pathlib import Path
import spikeinterface.core as sc
import spikeinterface.sorters as ss

# ---------- configuration ----------
RECORDING_PATH = Path("/data/snap/preprocessed")
SORTING_OUTPUT = Path("/data/snap/sorting_output")
SORTER = "kilosort4"            # or "mountainsort5", "spykingcircus2"
# -----------------------------------

# Load the preprocessed recording saved in Step 2
recording = sc.load(RECORDING_PATH)

# List installed sorters (sanity check)
print("Available sorters:", ss.available_sorters())

# Run the sorter
sorting = ss.run_sorter(
    sorter_name=SORTER,
    recording=recording,
    output_folder=SORTING_OUTPUT,
    remove_existing_folder=True,
    verbose=True,
)

print(f"Found {len(sorting.get_unit_ids())} units")
print(f"Unit IDs: {sorting.get_unit_ids()}")
```

### Sorter notes

* **Kilosort 4** requires a CUDA-capable GPU and the `kilosort` Python
  package.
* **MountainSort 5** is CPU-only and works well for low-channel-count probes.
* **SpykingCircus 2** ships inside `spikeinterface[full]` and needs no extra
  install.

> **Important:** `sc.load()` is the correct call for loading a previously saved
> SpikeInterface recording folder. Do **not** use the deprecated
> `read_binary_folder()`.

---

## Step 4 -- Quality Metrics

**Script:** `04_quality_metrics.py`

Starting with SpikeInterface 0.101 the old `WaveformExtractor` has been
replaced by **`SortingAnalyzer`**, which manages all post-sorting computations
(waveforms, templates, metrics) in a single object.

```python
from pathlib import Path
import spikeinterface.core as sc
import spikeinterface.sorters as ss

# ---------- configuration ----------
RECORDING_PATH = Path("/data/snap/preprocessed")
SORTING_PATH = Path("/data/snap/sorting_output")
ANALYZER_PATH = Path("/data/snap/sorting_analyzer")
# -----------------------------------

# Load recording and sorting result
recording = sc.load(RECORDING_PATH)
sorting = ss.read_sorter_folder(SORTING_PATH)

# Create a SortingAnalyzer (new API >= 0.101)
analyzer = sc.create_sorting_analyzer(
    sorting=sorting,
    recording=recording,
    format="binary_folder",
    folder=ANALYZER_PATH,
    sparse=True,
    overwrite=True,
)

# Compute extensions -- ORDER MATTERS (each may depend on the previous)
analyzer.compute("random_spikes")
analyzer.compute("waveforms")
analyzer.compute("templates")
analyzer.compute("noise_levels")
analyzer.compute("unit_locations")
analyzer.compute("spike_amplitudes")
analyzer.compute("correlograms")
analyzer.compute("template_similarity")
analyzer.compute("quality_metrics")

# Retrieve the metrics as a pandas DataFrame
metrics = analyzer.get_extension("quality_metrics")
df = metrics.get_data()
print(df.to_string())

# Quick summary of key columns
for col in ["isi_violations_ratio", "snr", "firing_rate"]:
    if col in df.columns:
        print(f"\n{col}:")
        print(df[col].describe())
```

### Extension computation order

The extensions must be computed in dependency order. The chain used above is:

```
random_spikes
  -> waveforms
       -> templates
       -> noise_levels
       -> unit_locations
       -> spike_amplitudes
  -> correlograms
  -> template_similarity
  -> quality_metrics (depends on waveforms, templates, noise_levels, spike_amplitudes)
```

### Key quality metrics

| Metric                  | Good unit guideline          |
| ----------------------- | ---------------------------- |
| `isi_violations_ratio`  | < 0.01 (< 1 % refractory)   |
| `snr`                   | > 5                          |
| `firing_rate`           | > 0.1 Hz (not silent)        |

> **Important:** Load the sorting with `ss.read_sorter_folder()`, **not**
> `se.read_sorter_folder()`. The latter does not exist.

---

## Step 5 -- Export to Phy for Manual Curation

**Script:** `05_export_to_phy.py`

```python
from pathlib import Path
import spikeinterface.core as sc
from spikeinterface.exporters import export_to_phy    # NOT sc.export_to_phy

# ---------- configuration ----------
ANALYZER_PATH = Path("/data/snap/sorting_analyzer")
PHY_OUTPUT = Path("/data/snap/phy_output")
# -----------------------------------

# Load the SortingAnalyzer saved in Step 4
analyzer = sc.load(ANALYZER_PATH)

# Phy requires PCA -- compute if not already present
if not analyzer.has_extension("principal_components"):
    analyzer.compute(
        "principal_components",
        n_components=5,
        mode="by_channel_local",
    )

# Export
export_to_phy(
    analyzer,
    output_folder=PHY_OUTPUT,
    copy_binary=True,
    remove_if_exists=True,
    verbose=True,
)

print(f"Phy output written to {PHY_OUTPUT}")
print("Launch Phy with:")
print(f"  phy template-gui {PHY_OUTPUT / 'params.py'}")
```

### Using Phy

```bash
phy template-gui /data/snap/phy_output/params.py
```

Inside Phy you can merge, split, and label clusters. When you are done, save
and re-import the curated sorting back into SpikeInterface:

```python
from spikeinterface.extractors import read_phy

sorting_curated = read_phy("/data/snap/phy_output")
```

---

## Combined Pipeline Script

The script below runs all five steps in sequence. Adjust the paths at the top
and choose your sorter.

```python
#!/usr/bin/env python3
"""
Full SpikeInterface pipeline for SNAP recordings.
Runs: load -> preprocess -> sort -> quality metrics -> export to Phy.
"""

from pathlib import Path
import spikeinterface.extractors as se
import spikeinterface.preprocessing as sp
import spikeinterface.core as sc
import spikeinterface.sorters as ss
from spikeinterface.exporters import export_to_phy

# ====================== CONFIGURATION ======================
RAW_RECORDING_PATH = Path("/data/snap/Record Node 101")
STREAM_ID          = "0"
PREPROCESSED_PATH  = Path("/data/snap/preprocessed")
SORTING_OUTPUT     = Path("/data/snap/sorting_output")
ANALYZER_PATH      = Path("/data/snap/sorting_analyzer")
PHY_OUTPUT         = Path("/data/snap/phy_output")
SORTER             = "kilosort4"   # "mountainsort5", "spykingcircus2"
# ============================================================


# ---- Step 1: Load and inspect ----
print("=" * 60)
print("Step 1: Loading recording")
print("=" * 60)

recording_raw = se.read_openephys(RAW_RECORDING_PATH, stream_id=STREAM_ID)

print(f"  Channels           : {recording_raw.get_num_channels()}")
print(f"  Sampling frequency : {recording_raw.get_sampling_frequency()} Hz")
print(f"  Duration           : {recording_raw.get_total_duration():.2f} s")
print(f"  Total samples      : {recording_raw.get_total_samples()}")
print(f"  Channel IDs        : {recording_raw.get_channel_ids()}")
print(f"  Has probe          : {recording_raw.has_probe()}")
if recording_raw.has_probe():
    print(f"  Probe              : {recording_raw.get_probe()}")


# ---- Step 2: Preprocess ----
print("\n" + "=" * 60)
print("Step 2: Preprocessing")
print("=" * 60)

recording_filtered = sp.bandpass_filter(
    recording_raw, freq_min=300.0, freq_max=6000.0, margin_ms=5.0,
)

recording_cmr = sp.common_reference(
    recording_filtered, reference="global", operator="median",
)

bad_channel_ids, bad_labels = sp.detect_bad_channels(
    recording_cmr, method="coherence+psd", seed=42,
)
print(f"  Bad channels: {bad_channel_ids}")

recording_clean = recording_cmr.remove_channels(bad_channel_ids)

recording_clean.save(
    folder=PREPROCESSED_PATH, format="binary", n_jobs=4, chunk_duration="1s",
)
print(f"  Saved to {PREPROCESSED_PATH}")


# ---- Step 3: Spike sorting ----
print("\n" + "=" * 60)
print(f"Step 3: Spike sorting ({SORTER})")
print("=" * 60)

recording_pp = sc.load(PREPROCESSED_PATH)

sorting = ss.run_sorter(
    sorter_name=SORTER,
    recording=recording_pp,
    output_folder=SORTING_OUTPUT,
    remove_existing_folder=True,
    verbose=True,
)
print(f"  Units found: {len(sorting.get_unit_ids())}")


# ---- Step 4: Quality metrics ----
print("\n" + "=" * 60)
print("Step 4: Quality metrics")
print("=" * 60)

recording_pp = sc.load(PREPROCESSED_PATH)
sorting = ss.read_sorter_folder(SORTING_OUTPUT)

analyzer = sc.create_sorting_analyzer(
    sorting=sorting,
    recording=recording_pp,
    format="binary_folder",
    folder=ANALYZER_PATH,
    sparse=True,
    overwrite=True,
)

for ext in [
    "random_spikes",
    "waveforms",
    "templates",
    "noise_levels",
    "unit_locations",
    "spike_amplitudes",
    "correlograms",
    "template_similarity",
    "quality_metrics",
]:
    print(f"  Computing {ext} ...")
    analyzer.compute(ext)

metrics_df = analyzer.get_extension("quality_metrics").get_data()
print("\n  Quality metrics summary:")
print(metrics_df.to_string())


# ---- Step 5: Export to Phy ----
print("\n" + "=" * 60)
print("Step 5: Export to Phy")
print("=" * 60)

analyzer = sc.load(ANALYZER_PATH)

if not analyzer.has_extension("principal_components"):
    analyzer.compute("principal_components", n_components=5, mode="by_channel_local")

export_to_phy(
    analyzer,
    output_folder=PHY_OUTPUT,
    copy_binary=True,
    remove_if_exists=True,
    verbose=True,
)

print(f"\n  Phy output: {PHY_OUTPUT}")
print(f"  Launch:  phy template-gui {PHY_OUTPUT / 'params.py'}")
print("\nPipeline complete.")
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `AttributeError: module 'spikeinterface.core' has no attribute 'read_binary_folder'` | Using deprecated 0.9x API | Replace with `sc.load(path)` |
| `AttributeError: module 'spikeinterface.core' has no attribute 'WaveformExtractor'` | Using deprecated 0.9x API | Use `sc.create_sorting_analyzer()` instead |
| `AttributeError: module 'spikeinterface.core' has no attribute 'export_to_phy'` | Wrong import path | Use `from spikeinterface.exporters import export_to_phy` |
| `AttributeError: module 'spikeinterface.extractors' has no attribute 'read_sorter_folder'` | Wrong module | Use `ss.read_sorter_folder()` (from `spikeinterface.sorters`) |
| `AttributeError: module 'spikeinterface.core' has no attribute 'load_sorting_analyzer'` | Old function name | Use `sc.load()` -- it detects the object type automatically |
| `ValueError: sorter 'kilosort4' is not installed` | Kilosort not in environment | `pip install kilosort` (needs CUDA) or switch to `"mountainsort5"` / `"spykingcircus2"` |
| `FileNotFoundError: settings.xml not found` | `RECORDING_PATH` wrong | Point to the Record Node directory that contains `settings.xml` |
| `ValueError: stream_id '0' not found` | Different stream numbering | Call `se.read_openephys(path)` without `stream_id` to see available streams |
| `No probe attached` message during load | Probe not saved with recording | Load a probe file via `probeinterface.read_probeinterface()` and attach with `recording.set_probe()` |
| `CUDA out of memory` during Kilosort 4 | GPU RAM too small | Reduce batch size in sorter params or use MountainSort 5 (CPU) |
| `quality_metrics` returns empty DataFrame | Missing upstream extensions | Compute all prerequisite extensions first: `random_spikes`, `waveforms`, `templates`, `noise_levels`, `spike_amplitudes` |
| `common_reference` produces ringing artefacts | Using `"average"` operator | Switch to `operator="median"` -- it is more robust to outlier channels |

---

## Further Reading

* [SpikeInterface documentation](https://spikeinterface.readthedocs.io/)
* [ProbeInterface documentation](https://probeinterface.readthedocs.io/)
* [Open Ephys data format](https://open-ephys.github.io/gui-docs/User-Manual/Recording-data/Open-Ephys-format.html)
* [Phy documentation](https://phy.readthedocs.io/)
