# Python SDK Reference (`snap-neuro`)

Python client for the SNAP JSON-RPC remote control API. This package provides
programmatic control of acquisition, recording, signal chain management, and
processor inspection from any Python 3.9+ environment.

| Detail              | Value                                      |
|---------------------|--------------------------------------------|
| Package name        | `snap-neuro`                               |
| Version             | `0.1.0`                                    |
| Python requirement  | `>=3.9`                                    |
| Dependencies        | `requests>=2.20.0`                         |
| License             | GPL-3.0                                    |
| Source location      | `Resources/PythonSDK/`                    |

---

## Installation

Install from the local source tree:

```bash
pip install ./Resources/PythonSDK
```

This installs the `snap` package and its sole dependency (`requests`).

---

## Quick Start

```python
from snap import SNAPClient

client = SNAPClient()  # connects to localhost:37497

if client.ping():
    print("SNAP is reachable")

    client.start_acquisition()
    client.start_recording("/data/experiment_001")

    # ... run your experiment ...

    client.stop_recording()
    client.stop_acquisition()
```

To connect to SNAP running on a different machine:

```python
client = SNAPClient(host="192.168.1.50", port=37497)
```

---

## API Reference

### `SNAPError`

Exception raised when SNAP returns a JSON-RPC error response.

```python
class SNAPError(Exception):
    def __init__(self, code: int, message: str, data: Any = None)
```

**Attributes:**

| Attribute | Type          | Description                              |
|-----------|---------------|------------------------------------------|
| `code`    | `int`         | Numeric error code from the JSON-RPC response |
| `message` | `str`         | Human-readable error description         |
| `data`    | `Any \| None` | Optional additional error data           |

**String representation:** `"SNAP Error {code}: {message}"`

**Example:**

```python
from snap import SNAPClient, SNAPError

client = SNAPClient()
try:
    client.start_recording("/nonexistent/path")
except SNAPError as e:
    print(e.code)     # numeric error code
    print(e.message)  # descriptive message
    print(e.data)     # additional data, or None
```

---

### `SNAPClient`

Client for SNAP's JSON-RPC remote control API. All communication uses JSON-RPC
2.0 over HTTP POST requests with a hardcoded 10-second timeout.

```python
class SNAPClient:
    def __init__(self, host: str = "localhost", port: int = 37497)
```

**Constructor parameters:**

| Parameter | Type  | Default       | Description                       |
|-----------|-------|---------------|-----------------------------------|
| `host`    | `str` | `"localhost"` | Hostname or IP where SNAP is running |
| `port`    | `int` | `37497`       | Port of SNAP's HTTP server        |

The constructor builds the endpoint URL as `http://{host}:{port}/api/rpc`. No
network connection is made until a method is called.

---

## Method Reference

### Acquisition

#### `start_acquisition()`

Start data acquisition.

```python
def start_acquisition(self) -> Dict
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client = SNAPClient()
result = client.start_acquisition()
```

---

#### `stop_acquisition()`

Stop data acquisition.

```python
def stop_acquisition(self) -> Dict
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client.stop_acquisition()
```

---

### Recording

#### `start_recording()`

Start recording data to disk.

```python
def start_recording(
    self,
    path: Optional[str] = None,
    create_new_directory: bool = True
) -> Dict
```

| Parameter              | Type            | Default | Description                                      |
|------------------------|-----------------|---------|--------------------------------------------------|
| `path`                 | `Optional[str]` | `None`  | Recording directory path. Uses SNAP's default if not specified. |
| `create_new_directory` | `bool`          | `True`  | Whether to create a new timestamped subdirectory. |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**JSON-RPC params sent:**

```json
{
  "path": "<path value or omitted if None>",
  "createNewDirectory": true
}
```

Note: when `path` is `None` (or any other falsy value), the `"path"` key is
omitted from the params dict sent to the server.

**Examples:**

```python
# Record to a specific directory, creating a new subdirectory
client.start_recording("/data/experiment_001")

# Record to the SNAP default location
client.start_recording()

# Record to a specific directory without creating a subdirectory
client.start_recording("/data/experiment_001", create_new_directory=False)
```

---

#### `stop_recording()`

Stop the active recording.

```python
def stop_recording(self) -> Dict
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client.stop_recording()
```

---

### Status

#### `get_status()`

Get the current SNAP status, including acquisition and recording state.

```python
def get_status(self) -> Dict
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `Dict` -- Status dictionary from the server (contains keys such as
`"acquiring"` and `"recording"`).

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
status = client.get_status()
print(status)
# e.g. {"acquiring": True, "recording": False, ...}
```

---

#### `is_acquiring()`

Check whether acquisition is currently active. This is a convenience wrapper
around `get_status()`.

```python
def is_acquiring(self) -> bool
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `bool` -- `True` if SNAP is currently acquiring data, `False`
otherwise. Internally reads `status.get("acquiring", False)`.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
if client.is_acquiring():
    print("Acquisition is running")
```

---

#### `is_recording()`

Check whether recording is currently active. This is a convenience wrapper
around `get_status()`.

```python
def is_recording(self) -> bool
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `bool` -- `True` if SNAP is currently recording, `False`
otherwise. Internally reads `status.get("recording", False)`.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
if not client.is_recording():
    client.start_recording()
```

---

### Processors

#### `list_processors()`

List all processors currently in the signal chain.

```python
def list_processors(self) -> List[Dict]
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `List[Dict]` -- List of processor dictionaries. Extracted from
`result.get("processors", [])`.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
processors = client.list_processors()
for proc in processors:
    print(proc)
```

---

#### `get_processor()`

Get details of a specific processor by its ID.

```python
def get_processor(self, processor_id: int) -> Dict
```

| Parameter      | Type  | Description                          |
|----------------|-------|--------------------------------------|
| `processor_id` | `int` | Numeric ID of the processor to query |

**Returns:** `Dict` -- Processor details from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error (e.g., processor not found) |

**Example:**

```python
proc = client.get_processor(100)
print(proc)
```

---

### Signal Chain

#### `load_signal_chain()`

Load a signal chain configuration from an XML file.

```python
def load_signal_chain(self, path: str) -> Dict
```

| Parameter | Type  | Description                           |
|-----------|-------|---------------------------------------|
| `path`    | `str` | Absolute path to the XML file to load |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client.load_signal_chain("/configs/my_chain.xml")
```

---

#### `save_signal_chain()`

Save the current signal chain configuration to an XML file.

```python
def save_signal_chain(self, path: str) -> Dict
```

| Parameter | Type  | Description                                    |
|-----------|-------|------------------------------------------------|
| `path`    | `str` | Absolute path where the XML file will be saved |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client.save_signal_chain("/configs/my_chain_backup.xml")
```

---

#### `clear_signal_chain()`

Remove all processors from the current signal chain.

```python
def clear_signal_chain(self) -> Dict
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client.clear_signal_chain()
```

---

### Plugins

#### `get_plugin_manifest()`

Get the manifest of all loaded plugins.

```python
def get_plugin_manifest(self) -> Dict
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `Dict` -- Plugin manifest dictionary from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
manifest = client.get_plugin_manifest()
print(manifest)
```

---

### Messages

#### `send_message()`

Send a broadcast message. Acquisition must be active for this call to succeed.

```python
def send_message(self, text: str) -> Dict
```

| Parameter | Type  | Description              |
|-----------|-------|--------------------------|
| `text`    | `str` | The message text to send |

**Returns:** `Dict` -- JSON-RPC result from the server.

**Raises:**

| Exception        | Condition                                  |
|------------------|--------------------------------------------|
| `ConnectionError` | Cannot connect to SNAP or HTTP error      |
| `TimeoutError`   | SNAP did not respond within 10 seconds     |
| `SNAPError`      | SNAP returned a JSON-RPC error             |

**Example:**

```python
client.start_acquisition()
client.send_message("stimulus onset")
```

---

### Convenience

#### `ping()`

Check whether SNAP is reachable. This calls `get_status()` internally and
catches connection-related exceptions.

```python
def ping(self) -> bool
```

| Parameter | Type | Description |
|-----------|------|-------------|
| *(none)*  |      |             |

**Returns:** `bool` -- `True` if SNAP responded successfully, `False` if a
`ConnectionError` or `TimeoutError` was raised.

Note: `SNAPError` is **not** caught by `ping()`. If the server returns a
JSON-RPC error, it will propagate.

**Example:**

```python
if client.ping():
    print("SNAP is online")
else:
    print("Cannot reach SNAP")
```

---

## Error Handling

Every method that communicates with the server can raise three categories of
exceptions:

| Exception        | When                                                       |
|------------------|------------------------------------------------------------|
| `ConnectionError` | SNAP is unreachable, HTTP error, or invalid JSON response |
| `TimeoutError`   | No response within 10 seconds                             |
| `SNAPError`      | SNAP returned a JSON-RPC error in the response body        |

`ConnectionError` and `TimeoutError` are Python built-ins. `SNAPError` is
provided by the `snap` package.

### Handling connection failures

```python
from snap import SNAPClient

client = SNAPClient()

try:
    client.start_acquisition()
except ConnectionError as e:
    print(f"Connection failed: {e}")
    # "Cannot connect to SNAP at http://localhost:37497/api/rpc.
    #  Is SNAP running with the HTTP server enabled?"
except TimeoutError as e:
    print(f"Timeout: {e}")
    # "SNAP did not respond within 10 seconds"
```

### Handling API errors

```python
from snap import SNAPClient, SNAPError

client = SNAPClient()

try:
    client.start_recording("/invalid/path")
except SNAPError as e:
    print(f"Error code: {e.code}")
    print(f"Message:    {e.message}")
    print(f"Data:       {e.data}")
```

### Comprehensive error handling pattern

```python
from snap import SNAPClient, SNAPError

client = SNAPClient()

try:
    status = client.get_status()
    print(f"Status: {status}")
except ConnectionError:
    print("SNAP is not running or not reachable")
except TimeoutError:
    print("SNAP is not responding")
except SNAPError as e:
    print(f"SNAP returned error {e.code}: {e.message}")
```

---

## Integration Example: Automated Recording with SpikeInterface Analysis

This example demonstrates a workflow that uses the SNAP Python SDK to control a
recording session, then hands off the recorded data to
[SpikeInterface](https://github.com/SpikeInterface/spikeinterface) for
spike-sorting analysis.

```python
"""
Automated recording session with post-hoc SpikeInterface analysis.

Requirements:
    pip install ./Resources/PythonSDK
    pip install spikeinterface[full]
"""

import time
from pathlib import Path
from snap import SNAPClient, SNAPError

# ------------------------------------------------------------------
# 1. Configure the session
# ------------------------------------------------------------------
RECORD_DIR = "/data/sessions/mouse_2024_01"
SIGNAL_CHAIN = "/configs/neuropixels_chain.xml"
RECORDING_DURATION_S = 300  # 5 minutes

client = SNAPClient()  # localhost:37497

# ------------------------------------------------------------------
# 2. Verify SNAP is reachable
# ------------------------------------------------------------------
if not client.ping():
    raise SystemExit("Cannot reach SNAP. Is it running?")

# ------------------------------------------------------------------
# 3. Load the signal chain and inspect processors
# ------------------------------------------------------------------
client.load_signal_chain(SIGNAL_CHAIN)

processors = client.list_processors()
print(f"Loaded {len(processors)} processors:")
for proc in processors:
    print(f"  - {proc}")

# ------------------------------------------------------------------
# 4. Run the recording
# ------------------------------------------------------------------
try:
    client.start_acquisition()
    client.start_recording(RECORD_DIR)
    client.send_message("experiment_start")

    print(f"Recording for {RECORDING_DURATION_S} seconds...")
    time.sleep(RECORDING_DURATION_S)

    client.send_message("experiment_end")
    client.stop_recording()
    client.stop_acquisition()

except SNAPError as e:
    print(f"SNAP error during recording: {e.code} - {e.message}")
    # Attempt graceful shutdown
    if client.is_recording():
        client.stop_recording()
    if client.is_acquiring():
        client.stop_acquisition()
    raise

print("Recording complete.")

# ------------------------------------------------------------------
# 5. Post-hoc analysis with SpikeInterface
# ------------------------------------------------------------------
import spikeinterface.full as si

# Load the recorded data (adjust reader to your format)
recording = si.read_openephys(RECORD_DIR)
print(f"Loaded recording: {recording.get_num_channels()} channels, "
      f"{recording.get_total_duration():.1f} seconds")

# Bandpass filter and common-average reference
recording_f = si.bandpass_filter(recording, freq_min=300, freq_max=6000)
recording_car = si.common_reference(recording_f, reference="global", operator="median")

# Run spike sorting
sorting = si.run_sorter(
    "kilosort3",
    recording_car,
    output_folder=Path(RECORD_DIR) / "kilosort3_output",
)
print(f"Found {len(sorting.get_unit_ids())} units")

# Extract waveforms and compute quality metrics
analyzer = si.create_sorting_analyzer(sorting, recording_car)
analyzer.compute("waveforms")
analyzer.compute("quality_metrics")

metrics = analyzer.get_extension("quality_metrics").get_data()
print(metrics)
```

---

## Method Summary

| Method                  | Returns      | JSON-RPC Method Called   |
|-------------------------|--------------|--------------------------|
| `start_acquisition()`   | `Dict`       | `acquisition.start`      |
| `stop_acquisition()`    | `Dict`       | `acquisition.stop`       |
| `start_recording()`     | `Dict`       | `recording.start`        |
| `stop_recording()`      | `Dict`       | `recording.stop`         |
| `get_status()`          | `Dict`       | `status.get`             |
| `is_acquiring()`        | `bool`       | *(calls `get_status()`)* |
| `is_recording()`        | `bool`       | *(calls `get_status()`)* |
| `list_processors()`     | `List[Dict]` | `processors.list`        |
| `get_processor(id)`     | `Dict`       | `processors.get`         |
| `load_signal_chain(p)`  | `Dict`       | `signalchain.load`       |
| `save_signal_chain(p)`  | `Dict`       | `signalchain.save`       |
| `clear_signal_chain()`  | `Dict`       | `signalchain.clear`      |
| `get_plugin_manifest()` | `Dict`       | `plugins.manifest`       |
| `send_message(text)`    | `Dict`       | `message.send`           |
| `ping()`                | `bool`       | *(calls `get_status()`)* |
