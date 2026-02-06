"""
Black & White Debug Server Client

TCP client library for communicating with the openblack debug server.
Connects to localhost:7777 and provides methods for all debug server commands.
"""

import json
import socket
import time
from typing import Any, Optional


class BWClient:
    """TCP client for the openblack debug server."""

    def __init__(self, host: str = "127.0.0.1", port: int = 7777, timeout: float = 5.0):
        self._host = host
        self._port = port
        self._timeout = timeout
        self._sock: Optional[socket.socket] = None
        self._file = None
        self._request_id = 0

    def connect(self) -> bool:
        """Connect to the debug server. Returns True on success."""
        try:
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._sock.settimeout(self._timeout)
            self._sock.connect((self._host, self._port))
            self._file = self._sock.makefile("r", encoding="utf-8")
            return True
        except (ConnectionRefusedError, socket.timeout, OSError) as e:
            print(f"Connection failed: {e}")
            self._sock = None
            self._file = None
            return False

    def disconnect(self):
        """Close the connection."""
        if self._file:
            try:
                self._file.close()
            except OSError:
                pass
            self._file = None
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    @property
    def connected(self) -> bool:
        return self._sock is not None

    def _send(self, method: str, params: Optional[dict] = None) -> dict:
        """Send a command and return the parsed response."""
        if not self._sock or not self._file:
            raise ConnectionError("Not connected to debug server")

        self._request_id += 1
        request: dict[str, Any] = {"id": self._request_id, "method": method}
        if params:
            request["params"] = params

        line = json.dumps(request) + "\n"
        self._sock.sendall(line.encode("utf-8"))

        response_line = self._file.readline()
        if not response_line:
            raise ConnectionError("Server closed connection")

        resp = json.loads(response_line)
        if not resp.get("ok"):
            error = resp.get("error", "Unknown error")
            raise RuntimeError(f"Server error for '{method}': {error}")

        return resp.get("result", {})

    # ---- State Queries ----

    def ping(self) -> dict:
        """Ping the server. Returns {pong: true, turn: N}."""
        return self._send("ping")

    def get_camera(self) -> dict:
        """Get camera state. Returns {origin, focus, rotation, fov}."""
        return self._send("get_camera")

    def get_hand(self) -> dict:
        """Get hand world position. Returns {position: [x, y, z]}."""
        return self._send("get_hand")

    def get_game_state(self) -> dict:
        """Get game state. Returns {turn, paused, speed}."""
        return self._send("get_game_state")

    def get_window_info(self) -> dict:
        """Get window dimensions. Returns {width, height}."""
        return self._send("get_window_info")

    def get_entities_near(self, x: float, y: float, z: float, radius: float = 500.0) -> dict:
        """Get entities within radius of a point. Returns {entities: [...]}."""
        return self._send("get_entities_near", {
            "x": x, "y": y, "z": z, "radius": radius
        })

    # ---- Input Injection ----

    def mouse_move(self, x: int, y: int):
        """Move the mouse cursor to screen coordinates (x, y)."""
        return self._send("mouse_move", {"x": x, "y": y})

    def mouse_click(self, x: int, y: int, button: str = "left", release: bool = True):
        """Click at screen coordinates.
        button: 'left', 'right', 'middle'
        If release=True (default), sends both DOWN and UP (a full click).
        If release=False, only presses down (for drag start).
        """
        # Always send the DOWN event
        self._send("mouse_click", {
            "x": x, "y": y, "button": button, "release": False
        })
        # If release=True, also send the UP event (completing the click)
        if release:
            return self._send("mouse_click", {
                "x": x, "y": y, "button": button, "release": True
            })

    def mouse_down(self, x: int, y: int, button: str = "left"):
        """Press mouse button down without releasing (for drag start)."""
        return self._send("mouse_click", {
            "x": x, "y": y, "button": button, "release": False
        })

    def mouse_up(self, x: int, y: int, button: str = "left"):
        """Release mouse button (for drag end)."""
        return self._send("mouse_click", {
            "x": x, "y": y, "button": button, "release": True
        })

    def mouse_scroll(self, delta: int):
        """Scroll the mouse wheel. Positive = up, negative = down."""
        return self._send("mouse_scroll", {"delta": delta})

    def key_press(self, key: str):
        """Press and release a key. Examples: 'a', 'space', 'escape', 'f1', 'up'."""
        return self._send("key_press", {"key": key})

    def drag(self, x1: int, y1: int, x2: int, y2: int,
             button: str = "left", steps: int = 10, delay: float = 0.02):
        """Drag from (x1,y1) to (x2,y2) with intermediate steps."""
        self.mouse_down(x1, y1, button)
        time.sleep(delay)

        for i in range(1, steps + 1):
            t = i / steps
            cx = int(x1 + (x2 - x1) * t)
            cy = int(y1 + (y2 - y1) * t)
            self.mouse_move(cx, cy)
            time.sleep(delay)

        self.mouse_up(x2, y2, button)

    # ---- Game Control ----

    def set_pause(self, paused: bool):
        """Pause or unpause the game."""
        return self._send("set_pause", {"paused": paused})

    def set_speed(self, speed: float):
        """Set game speed multiplier (1.0 = normal)."""
        return self._send("set_speed", {"speed": speed})

    def set_camera(self, origin: tuple = None, focus: tuple = None) -> dict:
        """Set camera position instantly.
        origin: (x, y, z) tuple for camera origin
        focus: (x, y, z) tuple for camera look-at point
        """
        params = {}
        if origin:
            params["origin_x"] = origin[0]
            params["origin_y"] = origin[1]
            params["origin_z"] = origin[2]
        if focus:
            params["focus_x"] = focus[0]
            params["focus_y"] = focus[1]
            params["focus_z"] = focus[2]
        return self._send("set_camera", params)

    # ---- Convenience ----

    def get_context_for_vision(self) -> dict:
        """Gather all state into a single dict for Claude Vision."""
        context = {}
        try:
            context["camera"] = self.get_camera()
        except Exception:
            context["camera"] = None
        try:
            context["hand"] = self.get_hand()
        except Exception:
            context["hand"] = None
        try:
            context["game_state"] = self.get_game_state()
        except Exception:
            context["game_state"] = None
        try:
            context["window"] = self.get_window_info()
        except Exception:
            context["window"] = None

        # Get entities near the camera origin if available
        try:
            cam = context.get("camera")
            if cam and cam.get("origin"):
                origin = cam["origin"]
                context["nearby_entities"] = self.get_entities_near(
                    origin[0], origin[1], origin[2], radius=500
                )
            else:
                context["nearby_entities"] = None
        except Exception:
            context["nearby_entities"] = None

        return context

    def wait_for_server(self, max_attempts: int = 30, interval: float = 2.0) -> bool:
        """Wait for the debug server to become available."""
        for i in range(max_attempts):
            if self.connect():
                return True
            print(f"  Waiting for debug server... (attempt {i + 1}/{max_attempts})")
            time.sleep(interval)
        return False


def main():
    """Quick self-test: connect and dump all state."""
    client = BWClient()
    if not client.connect():
        print("Could not connect to debug server on localhost:7777")
        print("Make sure the game is running.")
        return

    print("Connected to debug server!\n")

    try:
        print("=== Ping ===")
        print(json.dumps(client.ping(), indent=2))

        print("\n=== Camera ===")
        print(json.dumps(client.get_camera(), indent=2))

        print("\n=== Hand ===")
        print(json.dumps(client.get_hand(), indent=2))

        print("\n=== Game State ===")
        print(json.dumps(client.get_game_state(), indent=2))

        print("\n=== Window Info ===")
        print(json.dumps(client.get_window_info(), indent=2))

        print("\n=== Entities Near Camera ===")
        # Server now defaults to camera focus as center, 1000 radius
        ents = client.get_entities_near(0, 0, 0, 2000)
        totals = ents.get("totals", {})
        print(f"  Total in world: {totals.get('villagers', 0)} villagers, {totals.get('abodes', 0)} abodes, "
              f"{totals.get('creatures', 0)} creatures, {totals.get('trees', 0)} trees")
        print(f"  Query center: {ents.get('center', [])}, radius: {ents.get('radius', 0)}")
        print(f"  Found {ents.get('count', 0)} entities within radius")
        for e in ents.get("entities", [])[:10]:
            pos = e.get("pos", [0, 0, 0])
            print(f"    {e.get('type', '?')} #{e.get('id', '?')} at ({pos[0]:.0f}, {pos[1]:.0f}, {pos[2]:.0f})")
    finally:
        client.disconnect()
        print("\nDisconnected.")


if __name__ == "__main__":
    main()
