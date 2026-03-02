"""
Export Sorting Results to Phy for Manual Curation

Phy is a graphical tool for manually reviewing and curating spike sorting results.
This script exports SNAP sorting output to Phy format.

After export, launch Phy with:
    phy template-gui C:/data/experiment1/phy/params.py

Edit paths below.
"""

import spikeinterface.core as sc
from pathlib import Path

# ============================================================
# EDIT THESE
ANALYZER_PATH = r"C:\data\experiment1\analyzer"
PHY_OUTPUT = r"C:\data\experiment1\phy"
# ============================================================


def main():
    print("Loading SortingAnalyzer...")
    analyzer = sc.load_sorting_analyzer(ANALYZER_PATH)

    print(f"  {len(analyzer.sorting.get_unit_ids())} units")

    # Compute PCA if not already done (needed for Phy)
    if not analyzer.has_extension("principal_components"):
        print("Computing PCA features...")
        analyzer.compute(
            "principal_components", n_components=5, mode="by_channel_local"
        )

    # Export to Phy
    print(f"\nExporting to Phy format: {PHY_OUTPUT}")
    sc.export_to_phy(
        analyzer,
        output_folder=PHY_OUTPUT,
        copy_binary=True,
        remove_if_exists=True,
        verbose=True,
    )

    print(f"\nExport complete!")
    print(f"\nTo curate, run:")
    print(f"  phy template-gui {Path(PHY_OUTPUT) / 'params.py'}")
    print(f"\nAfter curation, re-import with:")
    print(f"  from spikeinterface.extractors import read_phy")
    print(f"  sorting_curated = read_phy('{PHY_OUTPUT}')")


if __name__ == "__main__":
    main()
