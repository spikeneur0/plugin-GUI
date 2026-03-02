# SNAP Analysis Scripts

Template scripts for post-recording analysis of SNAP data using SpikeInterface.

## Setup

```bash
pip install -r requirements.txt
```

## Workflow

1. `01_load_and_inspect.py` — Load a SNAP recording, print channel info, plot raw traces
2. `02_preprocessing.py` — Bandpass filter, common median reference, remove artifacts
3. `03_spike_sorting.py` — Run spike sorting (KiloSort4, MountainSort5, or SpyKING CIRCUS 2)
4. `04_quality_metrics.py` — Compute ISI violations, SNR, firing rate, amplitude cutoff
5. `05_export_to_phy.py` — Export to Phy for manual curation

## Usage

Edit the `RECORDING_PATH` variable at the top of each script to point to your SNAP recording directory.

Each script is self-contained and can be run independently, though they follow a natural workflow order.
