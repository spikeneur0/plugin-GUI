# SNAP Python SDK

Python client for controlling SNAP (Spike Neuro Acquisition Platform) remotely.

## Installation

```bash
pip install -e Resources/PythonSDK/
```

## Quick Start

```python
from snap import SNAPClient

client = SNAPClient()  # connects to localhost:37497

# Check connection
if client.ping():
    print("SNAP is running")

# Run an experiment
client.load_signal_chain("C:/experiments/my_chain.xml")
client.start_acquisition()
client.start_recording("C:/data/session001")

# ... experiment runs ...

client.stop_recording()
client.stop_acquisition()

# Query state
processors = client.list_processors()
manifest = client.get_plugin_manifest()
```

## API Reference

See `snap/client.py` for full method documentation.

## Requirements

- SNAP running with HTTP server enabled (default)
- Python 3.7+
- `requests` library
