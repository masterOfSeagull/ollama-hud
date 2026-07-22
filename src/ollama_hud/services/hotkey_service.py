from __future__ import annotations

import ctypes
from contextlib import suppress
from dataclasses import dataclass

VK_ESCAPE = 0x1B
VK_CONTROL = 0x11
VK_SHIFT = 0x10
VK_MENU = 0x12
VK_OEM_3 = 0xC0

MODIFIER_KEYS = {
    "Ctrl": (VK_CONTROL, 0xA2, 0xA3),
    "Shift": (VK_SHIFT, 0xA0, 0xA1),
    "Alt": (VK_MENU, 0xA4, 0xA5),
}

MODIFIER_ALIASES = {
    "ctrl": "Ctrl",
    "control": "Ctrl",
    "shift": "Shift",
    "alt": "Alt",
    "option": "Alt",
}

KEY_ALIASES = {
    "`": (VK_OEM_3, "`"),
    "~": (VK_OEM_3, "`"),
    "backtick": (VK_OEM_3, "`"),
    "grave": (VK_OEM_3, "`"),
    "esc": (VK_ESCAPE, "Esc"),
    "escape": (VK_ESCAPE, "Esc"),
    "space": (0x20, "Space"),
    "enter": (0x0D, "Enter"),
    "return": (0x0D, "Enter"),
    "tab": (0x09, "Tab"),
    "backspace": (0x08, "Backspace"),
    "delete": (0x2E, "Delete"),
    "del": (0x2E, "Delete"),
    "insert": (0x2D, "Insert"),
    "home": (0x24, "Home"),
    "end": (0x23, "End"),
    "pageup": (0x21, "PageUp"),
    "pagedown": (0x22, "PageDown"),
    "up": (0x26, "Up"),
    "down": (0x28, "Down"),
    "left": (0x25, "Left"),
    "right": (0x27, "Right"),
}


@dataclass(frozen=True)
class KeyboardShortcut:
    modifiers: frozenset[str]
    key_code: int
    key_name: str

    @property
    def display(self) -> str:
        modifiers = [name for name in ("Ctrl", "Shift", "Alt") if name in self.modifiers]
        return "+".join([*modifiers, self.key_name])

    def is_pressed(self) -> bool:
        return all(_any_key_down(MODIFIER_KEYS[name]) for name in self.modifiers) and _key_down(
            self.key_code
        )


EMERGENCY_EXIT_SHORTCUT = KeyboardShortcut(frozenset({"Ctrl"}), VK_OEM_3, "`")


class ShortcutLatch:
    def __init__(self, shortcut: KeyboardShortcut):
        self.shortcut = shortcut
        self.armed = True

    def consume_press(self) -> bool:
        pressed = self.shortcut.is_pressed()
        if not pressed:
            self.armed = True
            return False
        if not self.armed:
            return False
        self.armed = False
        return True


def parse_shortcut(text: str) -> KeyboardShortcut:
    parts = [part.strip() for part in text.split("+") if part.strip()]
    if not parts:
        raise ValueError("shortcut cannot be empty")

    modifiers: set[str] = set()
    key: tuple[int, str] | None = None
    for part in parts:
        lowered = part.lower()
        modifier = MODIFIER_ALIASES.get(lowered)
        if modifier:
            modifiers.add(modifier)
            continue
        if key is not None:
            raise ValueError(f"shortcut has multiple keys: {text}")
        key = _parse_key(lowered, part)

    if key is None:
        raise ValueError("shortcut must include a non-modifier key")
    key_code, key_name = key
    return KeyboardShortcut(frozenset(modifiers), key_code, key_name)


def exit_shortcut_pressed(shortcut: KeyboardShortcut | None) -> bool:
    if EMERGENCY_EXIT_SHORTCUT.is_pressed():
        return True
    return bool(shortcut and shortcut.is_pressed())


def _parse_key(lowered: str, original: str) -> tuple[int, str]:
    if lowered in KEY_ALIASES:
        return KEY_ALIASES[lowered]
    if len(original) == 1 and original.isalnum():
        return ord(original.upper()), original.upper()
    if lowered.startswith("f") and lowered[1:].isdigit():
        number = int(lowered[1:])
        if 1 <= number <= 24:
            return 0x6F + number, f"F{number}"
    raise ValueError(f"unsupported shortcut key: {original}")


def _key_down(key_code: int) -> bool:
    with suppress(Exception):
        return bool(ctypes.windll.user32.GetAsyncKeyState(key_code) & 0x8000)
    return False


def _any_key_down(key_codes: tuple[int, ...]) -> bool:
    return any(_key_down(key_code) for key_code in key_codes)
