# Changelog

All notable changes to SNAP (Spike Neuro Acquisition Platform) are documented
in this file. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

SNAP is a GPL-3.0 fork of the [Open Ephys GUI](https://github.com/open-ephys/plugin-GUI).
C++ GUI versioning follows the upstream scheme (`1.0.2`). The Python SDK
(`snap-neuro`) is versioned independently starting at `0.1.0`.

## [0.1.0] - 2026-03-03

### Added

- **JSON-RPC 2.0 HTTP server** — `POST /api/rpc` endpoint with 12 methods:
  `acquisition.start`, `acquisition.stop`, `status.get`, `processors.list`,
  `processors.get`, `signalchain.load`, `signalchain.save`, `signalchain.clear`,
  `recording.start`, `recording.stop`, `plugins.manifest`, `message.send`
- **Python SDK** (`snap-neuro`) — `SNAPClient` class with full JSON-RPC coverage,
  `SNAPError` exception, `ping()` convenience method
- **SpikeInterface analysis scripts** — five template scripts:
  `01_load_and_inspect.py`, `02_preprocessing.py`, `03_spike_sorting.py`,
  `04_quality_metrics.py`, `05_export_to_phy.py`
- **ProcessorState enum** — `IDLE`, `CONFIGURING`, `ACTIVE`, `ERRORED`,
  `DISABLED` states with color-coded graph viewer indicators
- **Node status indicators** — real-time processor state dots in the graph viewer
- **Canvas annotations** — draggable sticky notes on the graph viewer
- **Searchable processor toolbox** — always-visible search, source badges
  (`[SN]`/`[OE]`), persistent category collapse state
- **Plugin version manifest** — reproducible plugin configurations with
  `plugins.manifest` JSON-RPC method and XML save/load validation
- **ProbeInterface JSON support** — bundled Spike Neuro probe definitions,
  channel mapping standardization
- **User-facing documentation** — `docs/api-reference.md`, `docs/python-sdk.md`,
  `docs/spikeinterface-guide.md`, `docs/zmq-event-injector.md`
- **ACKNOWLEDGMENTS.md** — credits for Open Ephys GUI, Bonsai, SpikeGLX,
  SpikeInterface, ProbeInterface, and other referenced projects
- **CONTRIBUTING.md** — contributor guide with build instructions, coding
  conventions, and PR workflow
- **pyproject.toml** — modern Python packaging metadata for `snap-neuro`

### Changed

- Rebranded from Open Ephys GUI to SNAP (Spike Neuro Acquisition Platform)
- Updated copyright headers for GPL-3.0 compliance with fork attribution
- Restructured InfoLabel About text: added SNAP branding, source code links,
  contact information, and copyright notice
- Removed `onFocusLost` handler from processor list search field (prevented
  search from working due to layout-triggered focus loss)
- Bumped `python_requires` from `>=3.7` to `>=3.9` in `setup.py`
- Build system and installers rebranded (CMake project name, NSIS installer,
  CI/CD workflows)

### Fixed

- **Dangling promise UB** — converted all `std::promise` captures in
  `OpenEphysHttpServer.h` from stack-allocated `&done` references to
  `std::make_shared<std::promise<T>>` captured by value
- **Mixed locking deadlock** — unified `recording.start` handler to use
  single `callAsync` dispatch instead of mixing `MessageManagerLock` with
  `callAsync`
- **Thread-unsafe graph reads** — `status.get`, `processors.list`,
  `processors.get` now dispatch to message thread via shared_ptr
  promise/future pattern
- **Python import errors** — replaced nonexistent `se.read_binary_folder()`
  with `sc.load()`, moved `export_to_phy` to `spikeinterface.exporters`,
  moved `read_sorter_folder` to `spikeinterface.sorters`
- **HTTP/JSON error handling** — added `HTTPError` and `JSONDecodeError`
  handlers in `snap/client.py`
- **Plot index bounds** — fixed out-of-bounds indexing in
  `02_preprocessing.py` when bad channels are removed
- **Parameter type validation** — added JSON type checks for `id` (integer),
  `path` (string), `text` (string) across all RPC endpoints
- **`message.send` thread safety** — `broadcastMessage` now dispatched to
  message thread
- **Stale hyperlink coordinates** — cleaned up commented-out hyperlink code
  in `InfoLabel.cpp`, added DOI hyperlink
- **Incomplete publication citation** — added full authors, journal, year,
  and DOI to InfoLabel About text
- **ACKNOWLEDGMENTS OPETH URL** — corrected to point to actual OPETH
  repository instead of zmq-interface plugin
- **Build artifacts** — removed `build_output*.txt` from tracking, added to
  `.gitignore`
- **Recording panel layout** — clamped component bounds in
  `SNAPControlPanel.cpp` to prevent negative widths at small window sizes
- **Data viewport visibility** — clamped dimensions and extended visibility
  check in `SNAPUIComponent.cpp`
