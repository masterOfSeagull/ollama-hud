# Ollama HUD

Ollama HUD is a native Windows Qt 6/QML app that shows a click-through, topmost
overlay. It waits for the configured trigger shortcut, captures the primary
monitor, sends one screenshot to a local Ollama vision model, and displays a
short answer in the HUD panel.

The app is detection/advice only. It does not automate gameplay, read process
memory, inject input, capture active windows, or intercept mouse/keyboard input.

## Setup

1. Install Python 3.10 or newer.
2. Install and run Ollama locally.
3. Install Visual Studio Community with MSVC. The build helper can use Visual
   Studio's bundled CMake.
4. Pull at least one supported vision model:

   ```powershell
   ollama pull huihui_ai/qwen3-vl-abliterated:8b-instruct
   ollama pull gemma4:12b
   ```

5. Build or run the native app:

   ```powershell
   cd C:\projects\ollama-hud
   .\scripts\build_native.ps1
   ```

If Qt 6.9+ `msvc2022_64` is not found under `C:\Qt`, the build script installs
it with `aqtinstall`.

## Run

```powershell
.\run.bat
```

or:

```powershell
python -m ollama_hud
```

Use the `Model` field to choose an installed Ollama model, then use
`Start HUD`, switch back to the game or desktop, and press `Alt+1`.
Press `Alt+2` to clear the visible answer and wait for the next trigger.
Press `Esc` to close the HUD. `Ctrl+`` is always an emergency HUD exit.

`Q/A memory pairs` controls how many recent successful question/answer pairs
are included in the next chat request. The default is `3`; set it to `0` to
disable memory. The current screenshot is always attached, and each retained
prior turn is sent with its compact screenshot as stale context. Duplicate
repeated answers are collapsed before requesting.

`Instruction` controls the system instruction and prompt prefix sent with each
request. The default asks for one short action-only answer, but you can make it
more detailed or change the response style.

Settings are remembered between runs in `config/settings.yaml`. The GUI
autosaves valid field changes shortly after you edit them. `Return to Default`
resets the editable settings from `config/default.yaml` after confirmation.

Text-only request history is appended to `logs/chat.log`. Each entry includes
the timestamp, model, capture ID, question, included Q/A memory, answer or
error, retry status, screenshot attachment status, and chat message preview.
Screenshot payloads are not written to the log.

## Checks

```powershell
python -m pip install -r requirements-dev.txt
.\scripts\build_native.ps1 -Tests
pytest
ruff check .
python -m ollama_hud verify
```

`verify` reports local dependency and Ollama reachability status. It does not
start Ollama for you.

## Native compatibility

`run.bat`, `python -m ollama_hud`, `python -m ollama_hud verify`,
`python -m ollama_hud run`, and the `ollama-hud` console entry point now
delegate to `OllamaHud.exe`. If the executable is missing, the wrapper invokes
`scripts\build_native.ps1`.

## Third-party resources

The QML theme, reusable controls, fonts, icons, and image assets under
`third_party/genydl` are copied from GenyDL and remain under GPL-3.0-or-later.
See `third_party/genydl/ATTRIBUTION.md` and the copied license files.
