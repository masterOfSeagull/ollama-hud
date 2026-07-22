# Ollama HUD Architecture

Ollama HUD is organized as a layered desktop application while preserving the
existing Python, Tkinter, and local Ollama runtime.

## Layers

- `ollama_hud.core`: UI-neutral orchestration and runtime state. `HudController`
  owns request lifecycle, memory, state transitions, retries, and update polling.
- `ollama_hud.services`: integration services for screen capture, Ollama chat,
  hotkeys, settings, and structured chat logging.
- `ollama_hud.ui`: Tkinter control panel, click-through status overlay, and
  shared theme constants.
- `ollama_hud.resources`: package-owned resources for future bundled assets.
- `docs` and `packaging`: project documentation and distribution notes.

Top-level modules such as `runtime.py`, `gui.py`, `settings.py`, and
`ollama_client.py` remain compatibility shims during the transition.

## Data Flow

1. The UI or CLI creates `HudSettings` through `settings_service`.
2. The UI starts a `HudController`, optionally injecting service fakes in tests.
3. A trigger calls `start_request()`.
4. `capture_service` captures and encodes the current screenshot.
5. `ollama_service` builds the chat payload, sends it to Ollama, and records
   model thinking when returned.
6. `HudController` updates `RuntimeSnapshot`, stores bounded Q/A memory, and
   writes a text-only `chat_log_service` entry.
7. The UI polls `poll_updates()` and renders the current snapshot.

## Lifecycle

`HudController` exposes the stable app facade:

- `start_request()`
- `clear_visual_answer()`
- `poll_updates()`
- `start_overlay_loop()`
- `stop()`

The overlay loop is the only core path that imports Tkinter UI code, and it does
so lazily. `python -m ollama_hud verify` uses service interfaces only and does
not open a UI.

## Testing Boundaries

Core tests inject fake capture, client, and log services through `HudServices`.
Service tests cover payload construction, image encoding, settings validation,
hotkey parsing, and log formatting independently from Tkinter. UI tests should
focus on widget behavior and avoid asserting Ollama or capture details.
