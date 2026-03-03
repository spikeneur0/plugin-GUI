# SNAP Documentation

Developer documentation for the Spike Neuro Acquisition Platform.

## Contents

| Document | Description |
|----------|-------------|
| [API Reference](api-reference.md) | Complete HTTP API — JSON-RPC 2.0 and legacy REST endpoints |
| [Python SDK](python-sdk.md) | `snap-neuro` Python package for remote control |
| [SpikeInterface Guide](spikeinterface-guide.md) | End-to-end analysis pipeline: load, preprocess, sort, curate |
| [ZMQ Event Injector](zmq-event-injector.md) | Real-time event injection (planned; current alternatives documented) |

## Quick Start

```python
from snap.client import SNAPClient

client = SNAPClient()  # connects to localhost:37497

# Check connection
assert client.ping(), "SNAP is not running"

# Get current status
status = client.get_status()
print(status)  # {"acquiring": False, "recording": False, "mode": "IDLE", ...}

# Start acquisition
client.start_acquisition()

# Start recording to a specific directory
client.start_recording(path="C:/data/experiment1")

# Send a broadcast message to all processors
client.send_message("TRIAL_START id=1")

# Stop recording and acquisition
client.stop_recording()
client.stop_acquisition()
```

## Connection Info

- **Host:** `localhost` (127.0.0.1)
- **Port:** `37497`
- **JSON-RPC endpoint:** `POST http://localhost:37497/api/rpc`
- **Legacy REST base:** `http://localhost:37497/api/`

The HTTP server starts automatically when SNAP launches. It only accepts connections from localhost.

## Architecture

```
+------------------+        HTTP (port 37497)        +------------------+
|                  | ─────────────────────────────── |                  |
|  Python script   |   JSON-RPC 2.0 / REST API      |    SNAP GUI      |
|  (snap-neuro)    | ◄──────────────────────────────  |  (C++ / JUCE)   |
|                  |                                  |                  |
+------------------+                                  +------------------+
                                                             │
                                                      Signal Chain
                                                             │
                                                    ┌────────┴────────┐
                                                    │   Processors    │
                                                    │  (plugins)      │
                                                    └────────┬────────┘
                                                             │
                                                      Record Nodes
                                                             │
                                                    ┌────────┴────────┐
                                                    │  Binary / NWB   │
                                                    │  data files     │
                                                    └─────────────────┘
                                                             │
                                                    SpikeInterface
                                                             │
                                                    ┌────────┴────────┐
                                                    │  Analysis       │
                                                    │  (Python)       │
                                                    └─────────────────┘
```
