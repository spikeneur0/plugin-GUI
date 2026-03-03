# Contributing to SNAP

SNAP (Spike Neuro Acquisition Platform) is a GPL-3.0 fork of the
[Open Ephys GUI](https://github.com/open-ephys/plugin-GUI). Contributions
are welcome — bug reports, feature requests, documentation fixes, and code
changes alike.

## GPL-3.0 Obligations

SNAP is licensed under the GNU General Public License v3.0. By submitting a
pull request you agree that your contribution will be released under the same
license. All source files must retain the GPL-3.0 header and include fork
attribution where the original Open Ephys copyright applies.

## Reporting Bugs

[Open an issue](https://github.com/spikeneuro/snap/issues) with:

- SNAP version (shown in the bottom-right of the Graph tab)
- Operating system and version
- Steps to reproduce, expected vs. actual behaviour
- Any relevant log output or screenshots

## Building from Source

### Prerequisites

| Platform | Toolchain |
|----------|-----------|
| Windows  | Visual Studio 2019+ with C++ Desktop workload |
| macOS    | Xcode 12+ command-line tools |
| Linux    | GCC 9+ / Clang 10+, plus JUCE dependencies (see below) |

Linux packages (Ubuntu/Debian):

```bash
sudo apt install build-essential cmake libfreetype6-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libasound2-dev freeglut3-dev libcurl4-openssl-dev
```

### Build Steps

```bash
# Clone the repository
git clone https://github.com/spikeneuro/snap.git
cd snap

# CMake requires building inside the Build/ directory
cd Build
cmake -G "Unix Makefiles" ..    # or "Visual Studio 16 2019" on Windows
cmake --build . --config Release
```

The resulting binary is placed in `Build/Release/` (or `Build/Debug/`).

### Python SDK (snap-neuro)

```bash
cd Resources/PythonSDK
pip install -e ".[dev]"   # editable install
python -c "from snap.client import SNAPClient; print('OK')"
```

## Coding Conventions

### C++ (JUCE / Core Application)

- **Standard:** C++17
- **Style:** Allman braces, 4-space indentation (matching JUCE convention)
- **Thread safety:** Use `MessageManager::callAsync` or the shared-ptr
  promise/future pattern for anything that touches the signal chain:
  ```cpp
  auto done = std::make_shared<std::promise<json>>();
  MessageManager::callAsync([done, accessClass]() {
      // work on message thread
      done->set_value(result);
  });
  json result = done->get_future().get();
  ```
- **No raw `new`:** Prefer `std::unique_ptr`, `std::shared_ptr`, or JUCE
  `ScopedPointer` / `OwnedArray`.
- **Frozen directories:** Do **not** modify files in `Source/Processors/` or
  `Source/Plugins/Headers/` unless explicitly approved. These track upstream
  Open Ephys and modifications create merge conflicts.

### Python (SDK & Analysis Scripts)

- **Standard:** Python >= 3.9
- **Style:** PEP 8, 4-space indentation
- **Type hints:** Encouraged for public API methods
- **Imports:** stdlib → third-party → local, separated by blank lines

### Adding a JSON-RPC Method

1. Add the handler lambda in `OpenEphysHttpServer.h` inside `dispatchRpcMethod()`
2. Use the shared-ptr promise/future pattern for message-thread dispatch
3. Validate parameter types before use (`params["key"].is_string()`, etc.)
4. Return a JSON object on success; throw or return a JSON-RPC error on failure
5. Document the method in `docs/api-reference.md`
6. Add a corresponding `SNAPClient` method in `Resources/PythonSDK/snap/client.py`
7. Update `docs/python-sdk.md`

### Adding a Plugin

Follow the [Open Ephys Developer Guide](https://open-ephys.github.io/gui-docs/Developer-Guide/index.html)
and the [plugin tutorial](https://open-ephys.github.io/gui-docs/Tutorials/How-To-Make-Your-Own-Plugin.html).
SNAP uses the same plugin API as Open Ephys GUI, so existing OE plugins are
compatible.

### Adding an Analysis Script

Place new scripts in `Resources/AnalysisScripts/` with a numbered prefix
(e.g., `06_my_analysis.py`). Use SpikeInterface >= 0.101 APIs — see
`docs/spikeinterface-guide.md` for the current conventions.

## Pull Request Workflow

1. Fork the repository and create a feature branch from `main`
2. Keep commits focused — one logical change per commit
3. Run the build and verify no regressions
4. Open a PR against `main` with a clear description of what and why
5. Address review feedback; maintainers may squash-merge

## Contact

- Issues: [GitHub Issues](https://github.com/spikeneuro/snap/issues)
- Email: support@spikeneuro.com
- Website: [spikeneuro.com](https://spikeneuro.com)
