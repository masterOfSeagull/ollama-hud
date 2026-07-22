# Packaging

Ollama HUD currently targets direct Python execution on Windows.

Supported entry points:

- `python -m ollama_hud`
- `python -m ollama_hud verify`
- `python -m ollama_hud run`
- `ollama-hud`
- `run.bat`

For a bundled Windows build, keep `config/default.yaml` next to the application
root, preserve writable `config/settings.yaml` and `logs/chat.log`, and include
the optional Windows-only `dxcam` dependency. Installer and signing work is
deferred until explicitly requested.
