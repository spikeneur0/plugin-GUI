# SNAP API Reference

Complete JSON-RPC 2.0 and Legacy REST API reference for SNAP (Spike Neuro Acquisition Platform).

---

## Table of Contents

- [Connection](#connection)
- [JSON-RPC 2.0 API (Recommended)](#json-rpc-20-api-recommended)
  - [Endpoint](#endpoint)
  - [Request Format](#request-format)
  - [Response Format](#response-format)
  - [Error Codes](#error-codes)
  - [Methods](#methods)
    - [acquisition.start](#acquisitionstart)
    - [acquisition.stop](#acquisitionstop)
    - [status.get](#statusget)
    - [processors.list](#processorslist)
    - [processors.get](#processorsget)
    - [signalchain.load](#signalchainload)
    - [signalchain.save](#signalchainsave)
    - [signalchain.clear](#signalchainclear)
    - [recording.start](#recordingstart)
    - [recording.stop](#recordingstop)
    - [plugins.manifest](#pluginsmanifest)
    - [message.send](#messagesend)
- [Legacy REST API](#legacy-rest-api)
  - [Status and Control](#status-and-control)
  - [Audio Configuration](#audio-configuration)
  - [Recording Configuration](#recording-configuration)
  - [Signal Chain Management](#signal-chain-management)
  - [Processor Inspection and Configuration](#processor-inspection-and-configuration)
  - [Undo/Redo and Application Control](#undoredo-and-application-control)
- [Workflow Examples](#workflow-examples)
  - [Scripted Recording Session](#scripted-recording-session)
  - [Parameter Automation](#parameter-automation)
  - [Signal Chain Management Workflow](#signal-chain-management-workflow)

---

## Connection

| Property | Value |
|---|---|
| Host | `127.0.0.1` (localhost only) |
| Port | `37497` |
| Base URL | `http://127.0.0.1:37497` |

SNAP exposes two API layers on the same port:

1. **JSON-RPC 2.0** — The primary API, accessed via `POST /api/rpc`. Recommended for all new integrations.
2. **Legacy REST API** — GET/PUT endpoints under `/api/*`. Retained for backwards compatibility with existing Open Ephys GUI scripts and plugins.

---

## JSON-RPC 2.0 API (Recommended)

### Endpoint

```
POST http://127.0.0.1:37497/api/rpc
Content-Type: application/json
```

### Request Format

Every request must be a JSON object conforming to the JSON-RPC 2.0 specification:

```json
{
  "jsonrpc": "2.0",
  "method": "<method_name>",
  "params": {},
  "id": 1
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `jsonrpc` | string | Yes | Must be `"2.0"`. |
| `method` | string | Yes | The RPC method name. |
| `params` | object | No | Method-specific parameters. Omit or pass `{}` when none are required. |
| `id` | integer or string | Yes | Caller-assigned request identifier, echoed back in the response. |

### Response Format

**Success:**

```json
{
  "jsonrpc": "2.0",
  "result": { ... },
  "id": 1
}
```

**Error:**

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Method not found"
  },
  "id": 1
}
```

### Error Codes

| Code | Name | Description |
|---|---|---|
| `-32700` | Parse error | The request body is not valid JSON. |
| `-32600` | Invalid Request | The JSON object is missing the `method` field. |
| `-32601` | Method not found | The specified method does not exist. |
| `-32602` | Invalid params | A required parameter is missing or has the wrong type. |
| `-32603` | Internal error | An uncaught exception occurred during method execution. |
| `-32000` | Server error | A domain-level failure such as timeout, state conflict, or resource unavailability. |

---

### Methods

#### acquisition.start

Start data acquisition. The signal chain begins processing data from sources.

**Parameters:** none

**Response:**

| Field | Type | Values |
|---|---|---|
| `status` | string | `"started"`, `"already_running"`, or `"failed"` |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "acquisition.start",
    "params": {},
    "id": 1
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": { "status": "started" },
  "id": 1
}
```

**Timeout:** 5 seconds

---

#### acquisition.stop

Stop data acquisition. If recording is active, it is also stopped.

**Parameters:** none

**Response:**

| Field | Type | Values |
|---|---|---|
| `status` | string | `"stopped"` or `"already_stopped"` |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "acquisition.stop",
    "params": {},
    "id": 2
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": { "status": "stopped" },
  "id": 2
}
```

**Timeout:** 5 seconds

---

#### status.get

Get the current system status including acquisition state, recording state, processor count, and operating mode.

**Parameters:** none

**Response:**

| Field | Type | Description |
|---|---|---|
| `acquiring` | boolean | `true` if acquisition is active. |
| `recording` | boolean | `true` if recording is active. |
| `processorCount` | integer | Number of processors in the signal chain. |
| `mode` | string | One of `"IDLE"`, `"ACQUIRE"`, or `"RECORD"`. |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "status.get",
    "params": {},
    "id": 3
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "acquiring": true,
    "recording": false,
    "processorCount": 4,
    "mode": "ACQUIRE"
  },
  "id": 3
}
```

**Timeout:** 5 seconds

---

#### processors.list

List all processors currently in the signal chain.

**Parameters:** none

**Response:**

| Field | Type | Description |
|---|---|---|
| `processors` | array | Array of processor summary objects. |

Each processor object:

| Field | Type | Description |
|---|---|---|
| `id` | integer | Unique processor identifier. |
| `name` | string | Processor display name. |
| `state` | string | One of `"IDLE"`, `"CONFIGURING"`, `"ACTIVE"`, `"ERRORED"`, `"DISABLED"`, `"UNKNOWN"`. |
| `type` | string | Processor type classification. |
| `predecessor` | integer or null | ID of the preceding processor in the chain, or `null` if first. |
| `streamCount` | integer | Number of data streams on this processor. |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "processors.list",
    "params": {},
    "id": 4
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "processors": [
      {
        "id": 100,
        "name": "Neuropixels-PXI",
        "state": "IDLE",
        "type": "Source",
        "predecessor": null,
        "streamCount": 2
      },
      {
        "id": 101,
        "name": "Bandpass Filter",
        "state": "IDLE",
        "type": "Filter",
        "predecessor": 100,
        "streamCount": 2
      },
      {
        "id": 102,
        "name": "Record Node",
        "state": "IDLE",
        "type": "Sink",
        "predecessor": 101,
        "streamCount": 2
      }
    ]
  },
  "id": 4
}
```

**Timeout:** 5 seconds

---

#### processors.get

Get detailed information about a specific processor, including its parameters and streams.

**Parameters:**

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | integer | Yes | The processor ID. Must be an integer. |

**Response:**

| Field | Type | Description |
|---|---|---|
| `id` | integer | Processor ID. |
| `name` | string | Processor display name. |
| `state` | string | Processor state (see `processors.list` for values). |
| `predecessor` | integer or null | ID of the preceding processor. |
| `parameters` | array | Array of parameter objects. |
| `streams` | array | Array of stream objects. |

Each parameter object:

| Field | Type | Description |
|---|---|---|
| `name` | string | Parameter name. |
| `type` | string | Parameter data type. |
| `value` | string | Current value as a string. |

Each stream object:

| Field | Type | Description |
|---|---|---|
| `name` | string | Stream name. |
| `source_id` | integer | Source processor ID. |
| `sample_rate` | float | Sample rate in Hz. |
| `channel_count` | integer | Number of channels. |
| `parameters` | array | Array of stream-level parameter objects. |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "processors.get",
    "params": { "id": 100 },
    "id": 5
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "id": 100,
    "name": "Neuropixels-PXI",
    "state": "IDLE",
    "predecessor": null,
    "parameters": [
      { "name": "ap_gain", "type": "int", "value": "500" },
      { "name": "lfp_gain", "type": "int", "value": "250" }
    ],
    "streams": [
      {
        "name": "ProbeA-AP",
        "source_id": 100,
        "sample_rate": 30000.0,
        "channel_count": 384,
        "parameters": [
          { "name": "enable_filter", "type": "boolean", "value": "true" }
        ]
      }
    ]
  },
  "id": 5
}
```

**Errors:**

- `-32602` — `id` parameter missing or not an integer.
- `-32602` — Processor with the given ID not found.

---

#### signalchain.load

Load a signal chain configuration from an XML file on disk.

**Parameters:**

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | Yes | Absolute filesystem path to the XML file. |

**Response:**

| Field | Type | Description |
|---|---|---|
| `status` | string | `"loaded"` on success. |
| `path` | string | The path that was loaded. |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "signalchain.load",
    "params": { "path": "C:/experiments/my_config.xml" },
    "id": 6
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": "loaded",
    "path": "C:/experiments/my_config.xml"
  },
  "id": 6
}
```

**Errors:**

- `-32602` — `path` parameter missing or not a string.
- `-32000` — Acquisition is currently active (stop acquisition before loading).

**Timeout:** 5 seconds

---

#### signalchain.save

Save the current signal chain configuration to an XML file on disk.

**Parameters:**

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | Yes | Absolute filesystem path for the output XML file. |

**Response:**

| Field | Type | Description |
|---|---|---|
| `status` | string | `"saved"` on success or `"write_failed"` if the write operation failed. |
| `path` | string | The path that was written (or attempted). |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "signalchain.save",
    "params": { "path": "C:/experiments/saved_config.xml" },
    "id": 7
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": "saved",
    "path": "C:/experiments/saved_config.xml"
  },
  "id": 7
}
```

**Errors:**

- `-32602` — `path` parameter missing or not a string.
- `-32000` — A file already exists at the specified path.

**Timeout:** 5 seconds

---

#### signalchain.clear

Remove all processors from the signal chain.

**Parameters:** none

**Response:**

| Field | Type | Description |
|---|---|---|
| `status` | string | `"cleared"` |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "signalchain.clear",
    "params": {},
    "id": 8
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": { "status": "cleared" },
  "id": 8
}
```

**Errors:**

- `-32000` — Acquisition is currently active.

**Timeout:** 5 seconds

---

#### recording.start

Start recording data to disk. Acquisition must already be active.

**Parameters:**

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | No | Override the recording output directory. |
| `createNewDirectory` | boolean | No | If `true`, create a new subdirectory for this recording. |

**Response:**

| Field | Type | Description |
|---|---|---|
| `status` | string | `"recording"` |
| `directory` | string | The directory where data is being written. |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "recording.start",
    "params": {
      "path": "C:/data/experiment_001",
      "createNewDirectory": true
    },
    "id": 9
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": "recording",
    "directory": "C:/data/experiment_001/2026-03-03_14-30-00"
  },
  "id": 9
}
```

**Timeout:** 5 seconds

---

#### recording.stop

Stop an active recording. Acquisition continues running.

**Parameters:** none

**Response:**

| Field | Type | Values |
|---|---|---|
| `status` | string | `"stopped"` or `"already_stopped"` |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "recording.stop",
    "params": {},
    "id": 10
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": { "status": "stopped" },
  "id": 10
}
```

**Timeout:** 5 seconds

---

#### plugins.manifest

Retrieve the complete manifest of loaded plugins, including SNAP version and API version information.

**Parameters:** none

**Response:**

| Field | Type | Description |
|---|---|---|
| `snap_version` | string | SNAP application version string. |
| `api_version` | integer | Plugin API version number. |
| `plugins` | array | Array of plugin descriptor objects. |

Each plugin object:

| Field | Type | Description |
|---|---|---|
| `name` | string | Plugin name. |
| `version` | string | Plugin version string. |
| `type` | string | Plugin type (e.g., Source, Filter, Sink). |
| `api_version` | integer | Plugin API version the plugin was built against. |
| `library` | string (optional) | Shared library filename, if loaded from a dynamic library. |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "plugins.manifest",
    "params": {},
    "id": 11
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": {
    "snap_version": "1.0.0",
    "api_version": 9,
    "plugins": [
      {
        "name": "Neuropixels-PXI",
        "version": "0.5.0",
        "type": "Source",
        "api_version": 9,
        "library": "neuropixels-pxi.dll"
      },
      {
        "name": "Bandpass Filter",
        "version": "1.0.0",
        "type": "Filter",
        "api_version": 9
      }
    ]
  },
  "id": 11
}
```

**Errors:**

- `-32000` — PluginManager not available.

---

#### message.send

Broadcast a text message to all processors in the signal chain. Acquisition must be active.

**Parameters:**

| Field | Type | Required | Description |
|---|---|---|---|
| `text` | string | Yes | The message text to broadcast. |

**Response:**

| Field | Type | Description |
|---|---|---|
| `status` | string | `"sent"` |

**Example:**

```bash
curl -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "message.send",
    "params": { "text": "TRIAL_START id=42" },
    "id": 12
  }'
```

```json
{
  "jsonrpc": "2.0",
  "result": { "status": "sent" },
  "id": 12
}
```

**Errors:**

- `-32602` — `text` parameter missing or not a string.
- `-32000` — Acquisition is not active.

**Timeout:** 5 seconds

---

## Legacy REST API

These endpoints are retained for backwards compatibility with scripts written for the original Open Ephys GUI HTTP server. For new integrations, prefer the JSON-RPC 2.0 API above.

All endpoints are served at `http://127.0.0.1:37497/api/...`.

---

### Status and Control

#### GET /api/status

Get the current operating mode.

**Response:**

```json
{ "mode": "IDLE" }
```

Mode is one of `"IDLE"`, `"ACQUIRE"`, or `"RECORD"`.

---

#### PUT /api/status

Set the operating mode. Transitions between idle, acquisition, and recording.

**Request body:**

```json
{ "mode": "RECORD" }
```

| Field | Type | Values |
|---|---|---|
| `mode` | string | `"IDLE"`, `"ACQUIRE"`, or `"RECORD"` |

---

#### GET /api/config

Get the current signal chain configuration as XML.

**Response:**

```json
{ "info": "<SETTINGS>...</SETTINGS>" }
```

---

#### GET /api/cpu

Get current CPU usage.

**Response:**

```json
{ "usage": 23.5 }
```

| Field | Type | Description |
|---|---|---|
| `usage` | float | CPU usage percentage. |

---

#### GET /api/latency

Get per-processor, per-stream latency measurements.

**Response:**

```json
{
  "processors": [
    {
      "id": 100,
      "name": "Neuropixels-PXI",
      "streams": [
        { "name": "ProbeA-AP", "latency": 0.033 }
      ]
    }
  ]
}
```

---

### Audio Configuration

#### GET /api/audio/devices

List available audio devices grouped by type.

**Response:**

```json
{
  "devices": {
    "Windows Audio": ["Speakers (Realtek)", "Headphones"],
    "ASIO": ["ASIO4ALL v2"]
  }
}
```

---

#### GET /api/audio/device

Get the currently selected audio device and its configuration.

**Response:**

```json
{
  "device_type": "Windows Audio",
  "device_name": "Speakers (Realtek)",
  "sample_rate": 44100.0,
  "buffer_size": 1024,
  "available_sample_rates": [44100, 48000, 96000],
  "available_buffer_sizes": [256, 512, 1024, 2048]
}
```

---

#### PUT /api/audio

Update audio device settings. All fields are optional; only provided fields are changed.

**Request body:**

```json
{
  "device_type": "Windows Audio",
  "device_name": "Speakers (Realtek)",
  "sample_rate": 48000,
  "buffer_size": 512
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `device_type` | string | No | Audio device type name. |
| `device_name` | string | No | Audio device name. |
| `sample_rate` | integer | No | Sample rate in Hz. |
| `buffer_size` | integer | No | Buffer size in samples. |

---

### Recording Configuration

#### GET /api/recording

Get the current recording configuration.

**Response:**

```json
{
  "parent_directory": "C:/data",
  "base_text": "experiment",
  "prepend_text": "",
  "append_text": "",
  "default_record_engine": "Binary",
  "record_nodes": [
    {
      "id": 102,
      "parent_directory": "C:/data",
      "record_engine": "Binary"
    }
  ]
}
```

---

#### PUT /api/recording

Update recording configuration.

**Request body:**

```json
{
  "parent_directory": "C:/data/project_x",
  "prepend_text": "mouse01",
  "base_text": "session",
  "append_text": "probe_a",
  "default_record_engine": "Binary",
  "start_new_directory": "true"
}
```

All fields are optional.

---

#### PUT /api/recording/{id}

Set options for a specific Record Node by its processor ID.

**Request body:**

```json
{
  "parent_directory": "C:/data/separate_output",
  "record_engine": "NWB2"
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `parent_directory` | string | No | Override output directory for this node. |
| `record_engine` | string | No | Override record engine for this node. |

---

### Signal Chain Management

#### PUT /api/load

Load a signal chain from an XML file.

**Request body:**

```json
{ "path": "C:/experiments/my_config.xml" }
```

---

#### PUT /api/save

Save the current signal chain to an XML file.

**Request body:**

```json
{ "filepath": "C:/experiments/saved_config.xml" }
```

Note: This endpoint uses `"filepath"` (not `"path"`).

---

#### GET /api/processors/clear

Clear the signal chain, removing all processors.

**Response:**

```json
{ "info": "Signal chain cleared" }
```

---

#### GET /api/processors/list

List all available processor types that can be added to the signal chain. This returns processor types from loaded plugins, not the processors currently in the chain.

**Response:**

```json
{
  "processors": [
    { "name": "Neuropixels-PXI" },
    { "name": "Bandpass Filter" },
    { "name": "Record Node" }
  ]
}
```

---

#### PUT /api/processors/add

Add a processor to the signal chain.

**Request body:**

```json
{
  "name": "Bandpass Filter",
  "source_id": 100,
  "dest_id": 101
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | Yes | Processor name (must match a name from `GET /api/processors/list`). |
| `source_id` | integer | No | Processor to use as input source. |
| `dest_id` | integer | No | Processor to insert before. |

---

#### PUT /api/processors/delete

Remove a processor from the signal chain.

**Request body:**

```json
{ "id": 101 }
```

---

### Processor Inspection and Configuration

#### GET /api/processors

Get all processors currently in the signal chain.

**Response:**

```json
{
  "processors": [
    {
      "id": 100,
      "name": "Neuropixels-PXI"
    }
  ]
}
```

---

#### GET /api/processors/{id}

Get details of a single processor by ID.

---

#### GET /api/processors/{id}/parameters

Get all parameters for a processor.

**Response:**

```json
{
  "parameters": [
    { "name": "ap_gain", "type": "int", "value": "500" },
    { "name": "lfp_gain", "type": "int", "value": "250" }
  ]
}
```

---

#### GET /api/processors/{id}/parameters/{name}

Get a single parameter by name.

---

#### PUT /api/processors/{id}/parameters/{name}

Set a parameter value.

**Request body:**

```json
{ "value": 1000 }
```

---

#### GET /api/processors/{id}/streams/{index}

Get a specific data stream by zero-based index, including its parameters.

---

#### GET /api/processors/{id}/streams/{index}/parameters

Get all parameters for a specific stream.

---

#### GET /api/processors/{id}/streams/{index}/parameters/{name}

Get a single stream parameter by name.

---

#### PUT /api/processors/{id}/streams/{index}/parameters/{name}

Set a stream parameter value.

**Request body:**

```json
{ "value": "true" }
```

---

#### PUT /api/processors/{id}/config

Send a configuration message to a processor.

**Request body:**

```json
{ "text": "CONFIG key=value" }
```

---

### Undo/Redo and Application Control

#### GET /api/undo

Undo the last action in the signal chain editor.

---

#### GET /api/redo

Redo the last undone action in the signal chain editor.

---

#### PUT /api/message

Broadcast a message to all processors (acquisition must be active).

**Request body:**

```json
{ "text": "TRIAL_START id=42" }
```

---

#### PUT /api/quit

Quit the SNAP application.

---

## Workflow Examples

### Scripted Recording Session

A complete workflow for starting acquisition, recording a fixed-duration trial, and stopping cleanly.

```bash
# 1. Check that SNAP is idle
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"status.get","params":{},"id":1}'
# Expected: "mode": "IDLE"

# 2. Start acquisition
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"acquisition.start","params":{},"id":2}'
# Expected: "status": "started"

# 3. Start recording with a specific output directory
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc":"2.0",
    "method":"recording.start",
    "params":{"path":"C:/data/mouse01","createNewDirectory":true},
    "id":3
  }'
# Expected: "status": "recording"

# 4. Send a trial start marker
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"message.send","params":{"text":"TRIAL_START"},"id":4}'

# 5. Wait for trial duration
sleep 60

# 6. Send a trial end marker
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"message.send","params":{"text":"TRIAL_END"},"id":5}'

# 7. Stop recording
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"recording.stop","params":{},"id":6}'

# 8. Stop acquisition
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"acquisition.stop","params":{},"id":7}'
```

### Parameter Automation

Read and modify processor parameters while idle, then verify the signal chain.

```bash
# 1. List all processors in the signal chain
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"processors.list","params":{},"id":1}'

# 2. Inspect a specific processor's parameters and streams
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"processors.get","params":{"id":100},"id":2}'

# 3. Modify a processor parameter via the legacy REST API
curl -s -X PUT http://127.0.0.1:37497/api/processors/100/parameters/ap_gain \
  -H "Content-Type: application/json" \
  -d '{"value": 1000}'

# 4. Modify a stream-level parameter via the legacy REST API
curl -s -X PUT http://127.0.0.1:37497/api/processors/100/streams/0/parameters/enable_filter \
  -H "Content-Type: application/json" \
  -d '{"value": "false"}'

# 5. Verify the changes
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"processors.get","params":{"id":100},"id":3}'
```

### Signal Chain Management Workflow

Save, clear, and reload a signal chain configuration.

```bash
# 1. Save the current signal chain
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc":"2.0",
    "method":"signalchain.save",
    "params":{"path":"C:/configs/baseline.xml"},
    "id":1
  }'

# 2. Clear the signal chain
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"signalchain.clear","params":{},"id":2}'

# 3. Load a different configuration
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc":"2.0",
    "method":"signalchain.load",
    "params":{"path":"C:/configs/high_density.xml"},
    "id":3
  }'

# 4. Verify the loaded chain
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"processors.list","params":{},"id":4}'

# 5. Check plugin compatibility
curl -s -X POST http://127.0.0.1:37497/api/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"plugins.manifest","params":{},"id":5}'
```
