from __future__ import annotations

import pytest

from ollama_hud.services.settings_service import (
    HudSettings,
    load_settings,
    save_settings,
    validate_settings,
)


def test_settings_serialize_and_reload(tmp_path):
    path = tmp_path / "settings.yaml"
    settings = HudSettings(
        host="http://localhost:11434",
        model="vision:test",
        trigger_shortcut="Alt+2",
        exit_shortcut="Esc",
        clear_shortcut="Alt+3",
        screenshot_max_edge=900,
        timeout_seconds=42,
        memory_qa_pairs=5,
        instruction="Be specific and mention uncertainty.",
        query="Where is the exit?",
        keep_alive="10m",
        think=False,
        options={
            "temperature": 0.1,
            "top_p": 0.7,
            "num_predict": 4096,
            "num_ctx": 16384,
            "repeat_penalty": 1.25,
            "repeat_last_n": 256,
        },
    )

    save_settings(settings, path)
    loaded = load_settings(path)

    assert loaded == settings


def test_settings_validation_rejects_invalid_values_without_tkinter():
    with pytest.raises(ValueError, match="Instruction is required"):
        validate_settings(HudSettings(instruction=""))

    with pytest.raises(ValueError, match="between 0 and 20"):
        validate_settings(HudSettings(memory_qa_pairs=21))
