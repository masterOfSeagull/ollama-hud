from __future__ import annotations

import pytest

from ollama_hud.services.hotkey_service import EMERGENCY_EXIT_SHORTCUT, parse_shortcut


def test_parse_accepts_default_trigger():
    shortcut = parse_shortcut("Alt+1")

    assert shortcut.display == "Alt+1"


def test_parse_accepts_escape():
    shortcut = parse_shortcut("Esc")

    assert shortcut.display == "Esc"


def test_parse_accepts_ctrl_backtick():
    shortcut = parse_shortcut("Ctrl+`")

    assert shortcut == EMERGENCY_EXIT_SHORTCUT
    assert shortcut.display == "Ctrl+`"


def test_parse_rejects_modifier_only_shortcut():
    with pytest.raises(ValueError, match="non-modifier"):
        parse_shortcut("Alt")
