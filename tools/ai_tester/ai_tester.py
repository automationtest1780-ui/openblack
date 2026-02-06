"""
Black & White AI Beta Tester

Uses Claude Vision (via Anthropic API) to autonomously test the game.
Captures screenshots, queries game state, sends to Claude, executes returned actions.

Usage:
    set ANTHROPIC_API_KEY=sk-ant-...
    python ai_tester.py [--task "description of what to test"]

Examples:
    python ai_tester.py --task "Pan the camera by right-click-dragging. Report if the view changes."
    python ai_tester.py --task "Try scrolling to zoom in and out. Report what happens."
    python ai_tester.py --task "Look around the world and describe what you see."
    python ai_tester.py --task "Find a villager and try clicking on them."
"""

import argparse
import base64
import io
import json
import os
import sys
import time
from typing import Optional

# Fix Windows console encoding for Unicode output
if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

try:
    import anthropic
except ImportError:
    print("ERROR: anthropic package not installed. Run: pip install anthropic")
    sys.exit(1)

try:
    import mss
    import mss.tools
except ImportError:
    print("ERROR: mss package not installed. Run: pip install mss")
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow package not installed. Run: pip install Pillow")
    sys.exit(1)

from bw_client import BWClient


SYSTEM_PROMPT = """You are an AI beta tester for Black & White (2001), a god game where the player controls a divine hand over a 3D island world.

## Your Role
You are testing the openblack reimplementation of this game. You can see screenshots of the game and receive structured game state data. You execute actions by returning JSON commands.

## Game Controls (what you can do)
- **Left-click**: Select/interact with objects (villagers, buildings, etc.)
- **Right-click drag**: Pan the camera (hold right mouse, drag to rotate view)
- **Scroll wheel**: Zoom in/out
- **Middle-click drag**: Alternative camera control
- **Keyboard**: Various shortcuts (P = pause, ESC = menu, etc.)

## How to respond
For each step, analyze the screenshot and game state, then respond with:
1. **observation**: What you see in the screenshot (brief)
2. **reasoning**: What you want to test and why
3. **actions**: A JSON array of actions to execute
4. **done**: Set to true if you've completed the test task, false otherwise
5. **findings**: Summary of what you've found (populate when done=true)

## Action Format
Return a JSON object with this structure:
```json
{
    "observation": "I see a 3D island with green terrain and some buildings in the distance.",
    "reasoning": "I want to test camera panning by right-click-dragging to the left.",
    "actions": [
        {"action": "mouse_click", "x": 640, "y": 360, "button": "right", "release": false},
        {"action": "wait", "seconds": 0.1},
        {"action": "mouse_move", "x": 500, "y": 360},
        {"action": "wait", "seconds": 0.1},
        {"action": "mouse_move", "x": 400, "y": 360},
        {"action": "mouse_click", "x": 400, "y": 360, "button": "right", "release": true}
    ],
    "done": false,
    "findings": ""
}
```

## Available Actions
- `{"action": "mouse_click", "x": N, "y": N, "button": "left|right|middle", "release": true|false}`
- `{"action": "mouse_move", "x": N, "y": N}`
- `{"action": "mouse_scroll", "delta": N}` (positive = zoom in, negative = zoom out)
- `{"action": "key_press", "key": "p|escape|space|a-z|0-9|f1-f12|up|down|left|right"}`
- `{"action": "drag", "x1": N, "y1": N, "x2": N, "y2": N, "button": "left|right", "steps": 10}`
- `{"action": "wait", "seconds": N}`
- `{"action": "set_pause", "paused": true|false}`
- `{"action": "set_speed", "speed": N}`
- `{"action": "set_camera", "origin": [x, y, z], "focus": [x, y, z]}` (teleport camera instantly)

## Important Notes
- Screen coordinates: (0,0) is top-left. Check window_info for dimensions.
- The game may render a 3D world with terrain, buildings, villagers, creatures.
- The divine hand cursor may be visible on screen.
- If the screen is black or shows loading, wait and try again.
- Report any crashes, visual glitches, or unexpected behavior.
- Be methodical: try one thing at a time, observe the result, then try the next thing.
"""


_game_hwnd = None  # Cached game window handle


def find_game_window() -> Optional[tuple]:
    """Find the openblack game window, bring it to foreground, and return (left, top, width, height).
    Caches the window handle after first find to avoid picking up the console window on subsequent calls."""
    global _game_hwnd
    try:
        import ctypes
        from ctypes import wintypes

        user32 = ctypes.windll.user32
        GetClientRect = user32.GetClientRect
        ClientToScreen = user32.ClientToScreen
        SetForegroundWindow = user32.SetForegroundWindow
        ShowWindow = user32.ShowWindow
        IsWindow = user32.IsWindow

        def _get_window_rect(hwnd):
            """Get client area rect for a window handle."""
            client_rect = wintypes.RECT()
            GetClientRect(hwnd, ctypes.byref(client_rect))
            point = wintypes.POINT(0, 0)
            ClientToScreen(hwnd, ctypes.byref(point))
            return (point.x, point.y, client_rect.right, client_rect.bottom)

        # If we already have a cached handle and it's still valid, reuse it
        if _game_hwnd and IsWindow(_game_hwnd):
            rect = _get_window_rect(_game_hwnd)
            # Bring to front
            SetWindowPos = user32.SetWindowPos
            SWP_NOMOVE = 0x0002
            SWP_NOSIZE = 0x0001
            SWP_SHOWWINDOW = 0x0040
            HWND_TOPMOST = wintypes.HWND(-1)
            console_hwnd = ctypes.windll.kernel32.GetConsoleWindow()
            if console_hwnd:
                ShowWindow(wintypes.HWND(console_hwnd), 6)  # SW_MINIMIZE
            ShowWindow(_game_hwnd, 9)  # SW_RESTORE
            SetForegroundWindow(_game_hwnd)
            SetWindowPos(_game_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)
            time.sleep(0.15)
            return rect

        # First-time enumeration: find the SDL rendering window (not the console window)
        EnumWindows = user32.EnumWindows
        GetWindowTextW = user32.GetWindowTextW
        GetWindowTextLengthW = user32.GetWindowTextLengthW
        GetClassNameW = user32.GetClassNameW
        IsWindowVisible = user32.IsWindowVisible

        WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

        candidates = []

        def callback(hwnd, _lparam):
            if not IsWindowVisible(hwnd):
                return True
            length = GetWindowTextLengthW(hwnd)
            if length == 0:
                return True
            buf = ctypes.create_unicode_buffer(length + 1)
            GetWindowTextW(hwnd, buf, length + 1)
            title = buf.value
            if "openblack" in title.lower():
                cls_buf = ctypes.create_unicode_buffer(256)
                GetClassNameW(hwnd, cls_buf, 256)
                class_name = cls_buf.value
                # Exclude console/terminal window classes
                console_classes = {"ConsoleWindowClass", "CASCADIA_HOSTING_WINDOW_CLASS",
                                   "mintty", "PseudoConsoleWindow", "VirtualConsoleClass"}
                is_console = (class_name in console_classes)
                candidates.append((hwnd, class_name, is_console))
            return True

        EnumWindows(WNDENUMPROC(callback), 0)

        if not candidates:
            return None

        # Prefer non-console windows (the SDL rendering window)
        non_console = [c for c in candidates if not c[2]]
        if non_console:
            best_hwnd = non_console[0][0]
            best_class = non_console[0][1]
        else:
            # All candidates are console windows - list them for debugging
            for c in candidates:
                print(f"  Candidate: class='{c[1]}', console={c[2]}")
            best_hwnd = candidates[0][0]
            best_class = candidates[0][1]
            print(f"  WARNING: No non-console window found, using: class='{best_class}'")

        # Restore the window FIRST (it may be minimized), then get its rect
        ShowWindow(best_hwnd, 9)  # SW_RESTORE
        time.sleep(0.2)
        best_rect = _get_window_rect(best_hwnd)
        print(f"  Found game window: class='{best_class}', size={best_rect[2]}x{best_rect[3]}")
        _game_hwnd = best_hwnd

        SetWindowPos = user32.SetWindowPos
        SWP_NOMOVE = 0x0002
        SWP_NOSIZE = 0x0001
        SWP_SHOWWINDOW = 0x0040
        HWND_TOPMOST = wintypes.HWND(-1)

        # Minimize our own console window
        console_hwnd = ctypes.windll.kernel32.GetConsoleWindow()
        if console_hwnd:
            ShowWindow(wintypes.HWND(console_hwnd), 6)  # SW_MINIMIZE

        # Force game window to topmost temporarily
        ShowWindow(_game_hwnd, 9)  # SW_RESTORE
        SetForegroundWindow(_game_hwnd)
        SetWindowPos(_game_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)
        time.sleep(0.2)

        return best_rect
    except Exception as e:
        print(f"  Could not find game window: {e}")
        return None


def capture_screenshot(max_width: int = 1280) -> Optional[str]:
    """Capture the game window (or full monitor as fallback) and return base64-encoded PNG."""
    try:
        with mss.mss() as sct:
            # Try to find the game window specifically
            game_rect = find_game_window()
            if game_rect:
                left, top, width, height = game_rect
                monitor = {"left": left, "top": top, "width": width, "height": height}
                print(f"  (Capturing game window: {width}x{height} at {left},{top})")
            else:
                monitor = sct.monitors[1]  # Fallback to primary monitor
                print(f"  (Capturing full monitor as fallback)")

            screenshot = sct.grab(monitor)

            # Note: Don't remove TOPMOST or restore console between captures
            # as this can cause the game window to minimize itself

            img = Image.frombytes("RGB", screenshot.size, screenshot.rgb)

            # Resize if too large (saves API tokens)
            if img.width > max_width:
                ratio = max_width / img.width
                new_size = (max_width, int(img.height * ratio))
                img = img.resize(new_size, Image.LANCZOS)

            buffer = io.BytesIO()
            img.save(buffer, format="PNG", optimize=True)
            return base64.standard_b64encode(buffer.getvalue()).decode("utf-8")
    except Exception as e:
        print(f"  Screenshot failed: {e}")
        return None


def execute_actions(client: BWClient, actions: list[dict]):
    """Execute a list of actions via the debug server."""
    for action in actions:
        action_type = action.get("action", "")

        try:
            if action_type == "mouse_click":
                x = int(action.get("x", 0))
                y = int(action.get("y", 0))
                button = action.get("button", "left")
                release = action.get("release", True)
                client.mouse_click(x, y, button, release)
                print(f"    mouse_click({x}, {y}, {button}, release={release})")

            elif action_type == "mouse_move":
                x = int(action.get("x", 0))
                y = int(action.get("y", 0))
                client.mouse_move(x, y)
                print(f"    mouse_move({x}, {y})")

            elif action_type == "mouse_scroll":
                delta = int(action.get("delta", 0))
                client.mouse_scroll(delta)
                print(f"    mouse_scroll({delta})")

            elif action_type == "key_press":
                key = action.get("key", "")
                client.key_press(key)
                print(f"    key_press('{key}')")

            elif action_type == "drag":
                x1 = int(action.get("x1", 0))
                y1 = int(action.get("y1", 0))
                x2 = int(action.get("x2", 0))
                y2 = int(action.get("y2", 0))
                button = action.get("button", "left")
                steps = int(action.get("steps", 10))
                client.drag(x1, y1, x2, y2, button, steps)
                print(f"    drag({x1},{y1} -> {x2},{y2}, {button}, {steps} steps)")

            elif action_type == "wait":
                seconds = float(action.get("seconds", 0.5))
                time.sleep(seconds)
                print(f"    wait({seconds}s)")

            elif action_type == "set_pause":
                paused = action.get("paused", True)
                client.set_pause(paused)
                print(f"    set_pause({paused})")

            elif action_type == "set_speed":
                speed = float(action.get("speed", 1.0))
                client.set_speed(speed)
                print(f"    set_speed({speed})")

            elif action_type == "set_camera":
                origin = action.get("origin")
                focus = action.get("focus")
                result = client.set_camera(
                    origin=tuple(origin) if origin else None,
                    focus=tuple(focus) if focus else None,
                )
                print(f"    set_camera -> origin={result.get('origin')}, focus={result.get('focus')}")

            else:
                print(f"    Unknown action: {action_type}")

        except Exception as e:
            print(f"    Action error ({action_type}): {e}")


def ask_claude(
    api_client: anthropic.Anthropic,
    screenshot_b64: str,
    game_state: dict,
    task: str,
    history: list[dict],
    model: str = "claude-sonnet-4-20250514",
) -> dict:
    """Send screenshot + state to Claude and get actions back."""

    # Build the user message content
    content = []

    # Add screenshot
    content.append({
        "type": "image",
        "source": {
            "type": "base64",
            "media_type": "image/png",
            "data": screenshot_b64,
        }
    })

    # Add game state text
    state_text = f"## Game State\n```json\n{json.dumps(game_state, indent=2)}\n```"
    content.append({"type": "text", "text": state_text})

    # Add task reminder
    content.append({
        "type": "text",
        "text": f"## Current Test Task\n{task}\n\nRespond with a JSON object containing: observation, reasoning, actions, done, findings."
    })

    # Build messages: history + current
    messages = list(history)
    messages.append({"role": "user", "content": content})

    response = api_client.messages.create(
        model=model,
        max_tokens=2048,
        system=SYSTEM_PROMPT,
        messages=messages,
    )

    # Extract text from response
    text = ""
    for block in response.content:
        if block.type == "text":
            text += block.text

    # Parse JSON from response (may be wrapped in ```json blocks)
    text = text.strip()
    if text.startswith("```json"):
        text = text[7:]
    if text.startswith("```"):
        text = text[3:]
    if text.endswith("```"):
        text = text[:-3]
    text = text.strip()

    try:
        result = json.loads(text)
    except json.JSONDecodeError:
        # Try to find JSON in the response
        start = text.find("{")
        end = text.rfind("}") + 1
        if start >= 0 and end > start:
            result = json.loads(text[start:end])
        else:
            result = {
                "observation": text[:200],
                "reasoning": "Failed to parse response as JSON",
                "actions": [],
                "done": False,
                "findings": "",
            }

    # Add assistant response to history for multi-turn context
    history.append({"role": "user", "content": content})
    history.append({"role": "assistant", "content": text})

    return result


def run_test(
    task: str,
    max_steps: int = 20,
    model: str = "claude-sonnet-4-20250514",
    delay_between_steps: float = 1.0,
):
    """Run an AI test session."""
    print(f"Task: {task}")
    print(f"Model: {model}")
    print(f"Max steps: {max_steps}")
    print("=" * 60)

    # Check API key
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        print("ERROR: ANTHROPIC_API_KEY environment variable not set.")
        print("Set it with: set ANTHROPIC_API_KEY=sk-ant-...")
        return

    api_client = anthropic.Anthropic(api_key=api_key)

    # Connect to game (with retry logic)
    client = BWClient()
    print("\nConnecting to debug server...")
    if not client.wait_for_server(max_attempts=30, interval=2.0):
        print("Could not connect after retrying. Make sure the game is running.")
        return

    print("Connected!\n")

    # Verify connection
    try:
        ping = client.ping()
        print(f"Server responding. Turn: {ping.get('turn', '?')}")
    except Exception as e:
        print(f"Ping failed: {e}")
        client.disconnect()
        return

    # Position camera near the village on Land 1 so AI can see the game world
    try:
        cam = client.get_camera()
        if cam.get("origin") and cam["origin"][1] > 200:
            print("Camera is too high (scripted cinematic view). Repositioning near village...")
            client.set_camera(origin=(2470, 80, 2400), focus=(2470, 10, 2550))
            time.sleep(0.3)
            cam = client.get_camera()
            print(f"Camera now at: ({cam['origin'][0]:.0f}, {cam['origin'][1]:.0f}, {cam['origin'][2]:.0f})")
    except Exception as e:
        print(f"Camera positioning note: {e}")

    print()
    history: list[dict] = []

    try:
        for step in range(1, max_steps + 1):
            print(f"--- Step {step}/{max_steps} ---")

            # Capture screenshot
            print("  Capturing screenshot...")
            screenshot = capture_screenshot()
            if not screenshot:
                print("  No screenshot, waiting...")
                time.sleep(2)
                continue

            # Get game state
            print("  Querying game state...")
            game_state = client.get_context_for_vision()

            # Ask Claude
            print("  Asking Claude...")
            try:
                result = ask_claude(api_client, screenshot, game_state, task, history, model)
            except Exception as e:
                print(f"  Claude API error: {e}")
                time.sleep(3)
                continue

            # Display Claude's thinking
            print(f"  Observation: {result.get('observation', '?')}")
            print(f"  Reasoning: {result.get('reasoning', '?')}")

            # Execute actions
            actions = result.get("actions", [])
            if actions:
                print(f"  Executing {len(actions)} actions:")
                execute_actions(client, actions)
            else:
                print("  No actions to execute.")

            # Check if done
            if result.get("done"):
                findings = result.get("findings", "No findings reported.")
                print(f"\n{'=' * 60}")
                print(f"TEST COMPLETE after {step} steps")
                print(f"{'=' * 60}")
                print(f"\nFindings:\n{findings}")
                return

            # Wait between steps
            time.sleep(delay_between_steps)

        print(f"\nReached max steps ({max_steps}). Test incomplete.")

    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")
    finally:
        client.disconnect()
        print("Disconnected from debug server.")


def main():
    parser = argparse.ArgumentParser(
        description="AI Beta Tester for Black & White (openblack)"
    )
    parser.add_argument(
        "--task",
        type=str,
        default="Explore the game world. Pan the camera, zoom in and out, and describe what you see. Report any visual issues or missing elements.",
        help="Description of what to test",
    )
    parser.add_argument(
        "--max-steps",
        type=int,
        default=20,
        help="Maximum number of test steps (default: 20)",
    )
    parser.add_argument(
        "--model",
        type=str,
        default="claude-sonnet-4-20250514",
        help="Claude model to use (default: claude-sonnet-4-20250514)",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=1.5,
        help="Delay between steps in seconds (default: 1.5)",
    )

    args = parser.parse_args()
    run_test(args.task, args.max_steps, args.model, args.delay)


if __name__ == "__main__":
    main()
