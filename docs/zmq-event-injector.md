# ZMQ Event Injector

> **Status:** Not yet available in SNAP.

The ZMQ Event Injector plugin is not currently included in the SNAP codebase. It is planned for a future release to enable closed-loop experiment paradigms.

## Background

The [ZMQ Interface plugin](https://github.com/open-ephys-plugins/zmq-interface) for Open Ephys GUI provides ZeroMQ-based data streaming and event injection. SNAP does not currently bundle this plugin, but it can be installed separately as a third-party plugin if needed.

## Current Alternatives for Real-Time Control

### HTTP API + Broadcast Messages

SNAP's built-in HTTP API can be used for real-time event injection during acquisition:

```python
from snap.client import SNAPClient

client = SNAPClient()

# Acquisition must be active to send messages
client.start_acquisition()

# Send a broadcast message to all processors in the signal chain
client.send_message("STIM_ON trial=5")

# Messages are received by processors that implement handleBroadcastMessage()
```

### Processor Config Messages (REST API)

For processor-specific control, use the REST endpoint:

```bash
# Send a config message to a specific processor (e.g., processor 105)
curl -X PUT http://localhost:37497/api/processors/105/config \
  -H "Content-Type: application/json" \
  -d '{"text": "trigger_output 1"}'
```

### Parameter Control

Modify processor parameters in real time during acquisition:

```bash
# Set a parameter value on a specific processor
curl -X PUT http://localhost:37497/api/processors/105/parameters/threshold \
  -H "Content-Type: application/json" \
  -d '{"value": 3.5}'

# Set a stream-specific parameter
curl -X PUT http://localhost:37497/api/processors/105/streams/0/parameters/gate \
  -H "Content-Type: application/json" \
  -d '{"value": 1}'
```

## Latency Considerations

| Method | Typical Latency | Use Case |
|--------|----------------|----------|
| HTTP broadcast message | 1-10 ms | Event markers, trial annotations |
| HTTP parameter change | 1-10 ms | Threshold adjustments, gate toggling |
| HTTP config message | 1-10 ms | Processor-specific commands |

These latencies are dominated by HTTP round-trip time over localhost. For sub-millisecond closed-loop control, a dedicated plugin with direct memory access (such as a future ZMQ plugin) would be required.

## Future Plans

When ZMQ support is added to SNAP, it will likely provide:

- **Data streaming** — Subscribe to continuous data, spikes, and events via ZMQ PUB/SUB
- **Event injection** — Push TTL events and messages into the signal chain via ZMQ PUSH/PULL
- **Low-latency closed-loop** — Bypass HTTP overhead for microsecond-scale feedback

See the [Open Ephys ZMQ Interface documentation](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/ZMQ-Interface.html) for reference on the protocol design that a future SNAP implementation may follow.
