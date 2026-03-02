"""
SNAP Remote Control Client

Connects to SNAP's JSON-RPC API for programmatic control of
acquisition, recording, and signal chain management.

Usage:
    from snap import SNAPClient

    client = SNAPClient()  # localhost:37497 by default
    client.start_acquisition()
    client.start_recording("/path/to/data")
    # ... run experiment ...
    client.stop_recording()
    client.stop_acquisition()
"""

import requests
import json
from typing import Optional, Dict, Any, List


class SNAPError(Exception):
    """Error returned by SNAP's JSON-RPC API."""

    def __init__(self, code: int, message: str, data: Any = None):
        self.code = code
        self.message = message
        self.data = data
        super().__init__(f"SNAP Error {code}: {message}")


class SNAPClient:
    """Client for SNAP's JSON-RPC remote control API."""

    def __init__(self, host: str = "localhost", port: int = 37497):
        self.url = f"http://{host}:{port}/api/rpc"
        self._id = 0

    def _call(self, method: str, params: Optional[Dict] = None) -> Any:
        """Send a JSON-RPC 2.0 request and return the result."""
        self._id += 1
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "id": self._id,
        }
        if params:
            payload["params"] = params

        try:
            response = requests.post(
                self.url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=10,
            )
            response.raise_for_status()
        except requests.ConnectionError:
            raise ConnectionError(
                f"Cannot connect to SNAP at {self.url}. "
                "Is SNAP running with the HTTP server enabled?"
            )
        except requests.Timeout:
            raise TimeoutError("SNAP did not respond within 10 seconds")
        except requests.HTTPError as e:
            raise ConnectionError(f"HTTP error from SNAP: {e}")

        try:
            result = response.json()
        except ValueError:
            raise ConnectionError(
                f"Invalid JSON response from SNAP (status {response.status_code})"
            )

        if "error" in result:
            err = result["error"]
            raise SNAPError(err["code"], err["message"], err.get("data"))

        return result.get("result")

    # === Acquisition ===

    def start_acquisition(self) -> Dict:
        """Start data acquisition."""
        return self._call("acquisition.start")

    def stop_acquisition(self) -> Dict:
        """Stop data acquisition."""
        return self._call("acquisition.stop")

    # === Recording ===

    def start_recording(
        self, path: Optional[str] = None, create_new_directory: bool = True
    ) -> Dict:
        """Start recording data to disk.

        Args:
            path: Recording directory path. Uses default if not specified.
            create_new_directory: Whether to create a new subdirectory.
        """
        params = {}
        if path:
            params["path"] = path
        params["createNewDirectory"] = create_new_directory
        return self._call("recording.start", params)

    def stop_recording(self) -> Dict:
        """Stop recording."""
        return self._call("recording.stop")

    # === Status ===

    def get_status(self) -> Dict:
        """Get current SNAP status (acquiring, recording, processor count)."""
        return self._call("status.get")

    def is_acquiring(self) -> bool:
        """Check if acquisition is active."""
        return self.get_status().get("acquiring", False)

    def is_recording(self) -> bool:
        """Check if recording is active."""
        return self.get_status().get("recording", False)

    # === Processors ===

    def list_processors(self) -> List[Dict]:
        """List all processors in the signal chain."""
        result = self._call("processors.list")
        return result.get("processors", [])

    def get_processor(self, processor_id: int) -> Dict:
        """Get details of a specific processor."""
        return self._call("processors.get", {"id": processor_id})

    # === Signal Chain ===

    def load_signal_chain(self, path: str) -> Dict:
        """Load a signal chain from an XML file."""
        return self._call("signalchain.load", {"path": path})

    def save_signal_chain(self, path: str) -> Dict:
        """Save the current signal chain to an XML file."""
        return self._call("signalchain.save", {"path": path})

    def clear_signal_chain(self) -> Dict:
        """Clear the current signal chain."""
        return self._call("signalchain.clear")

    # === Plugins ===

    def get_plugin_manifest(self) -> Dict:
        """Get the manifest of all loaded plugins."""
        return self._call("plugins.manifest")

    # === Messages ===

    def send_message(self, text: str) -> Dict:
        """Send a broadcast message (acquisition must be active)."""
        return self._call("message.send", {"text": text})

    # === Convenience ===

    def ping(self) -> bool:
        """Check if SNAP is reachable."""
        try:
            self.get_status()
            return True
        except (ConnectionError, TimeoutError):
            return False
