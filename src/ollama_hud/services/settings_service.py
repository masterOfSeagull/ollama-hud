from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml

from ollama_hud.services.hotkey_service import parse_shortcut

PROJECT_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_CONFIG_PATH = PROJECT_ROOT / "config" / "default.yaml"
USER_CONFIG_PATH = PROJECT_ROOT / "config" / "settings.yaml"
CHAT_LOG_PATH = PROJECT_ROOT / "logs" / "chat.log"

DEFAULT_HOST = "http://127.0.0.1:11434"
DEFAULT_MODEL = "huihui_ai/qwen3-vl-abliterated:8b-instruct"
AVAILABLE_MODELS = (
    DEFAULT_MODEL,
    "gemma4:12b",
)
DEFAULT_TRIGGER_SHORTCUT = "Alt+1"
DEFAULT_EXIT_SHORTCUT = "Esc"
DEFAULT_CLEAR_SHORTCUT = "Alt+2"
DEFAULT_MEMORY_QA_PAIRS = 3
DEFAULT_INSTRUCTION = (
    "Answer in one short sentence. No chain of thought. Give the best direction/action only."
)
DEFAULT_QUERY = (
    "In this RPG dungeon screenshot, identify the entrance, portal, exit, or door I "
    "should use next. Which direction or action should I take?"
)
DEFAULT_OPTIONS = {
    "temperature": 0.2,
    "top_p": 0.8,
    "num_predict": 2048,
    "num_ctx": 32768,
    "repeat_penalty": 1.1,
    "repeat_last_n": 64,
}


@dataclass(frozen=True)
class HudSettings:
    host: str = DEFAULT_HOST
    model: str = DEFAULT_MODEL
    trigger_shortcut: str = DEFAULT_TRIGGER_SHORTCUT
    exit_shortcut: str = DEFAULT_EXIT_SHORTCUT
    clear_shortcut: str = DEFAULT_CLEAR_SHORTCUT
    screenshot_max_edge: int = 1280
    timeout_seconds: float = 120.0
    memory_qa_pairs: int = DEFAULT_MEMORY_QA_PAIRS
    instruction: str = DEFAULT_INSTRUCTION
    query: str = DEFAULT_QUERY
    keep_alive: str = "30m"
    think: bool = True
    options: dict[str, int | float | str | bool] = field(
        default_factory=lambda: dict(DEFAULT_OPTIONS)
    )

    def normalized_host(self) -> str:
        return self.host.rstrip("/")


def load_settings(path: str | Path | None = None) -> HudSettings:
    config_path = Path(path) if path is not None else _preferred_config_path()
    data = _read_yaml_dict(config_path)
    return settings_from_dict(data)


def save_settings(settings: HudSettings, path: str | Path = USER_CONFIG_PATH) -> None:
    validate_settings(settings)
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        yaml.safe_dump(settings_to_dict(settings), sort_keys=False),
        encoding="utf-8",
    )


def settings_from_dict(data: dict[str, Any]) -> HudSettings:
    options = dict(DEFAULT_OPTIONS)
    raw_options = data.get("options")
    if isinstance(raw_options, dict):
        for key, value in raw_options.items():
            if isinstance(key, str) and isinstance(value, int | float | str | bool):
                options[key] = value

    return HudSettings(
        host=_string(data.get("host"), DEFAULT_HOST),
        model=_string(data.get("model"), DEFAULT_MODEL),
        trigger_shortcut=_string(data.get("trigger_shortcut"), DEFAULT_TRIGGER_SHORTCUT),
        exit_shortcut=_string(data.get("exit_shortcut"), DEFAULT_EXIT_SHORTCUT),
        clear_shortcut=_string(data.get("clear_shortcut"), DEFAULT_CLEAR_SHORTCUT),
        screenshot_max_edge=max(64, _int(data.get("screenshot_max_edge"), 1280)),
        timeout_seconds=max(1.0, _float(data.get("timeout_seconds"), 120.0)),
        memory_qa_pairs=max(0, _int(data.get("memory_qa_pairs"), DEFAULT_MEMORY_QA_PAIRS)),
        instruction=_string(data.get("instruction"), DEFAULT_INSTRUCTION),
        query=_string(data.get("query"), DEFAULT_QUERY),
        keep_alive=_string(data.get("keep_alive"), "30m"),
        think=_bool(data.get("think"), True),
        options=options,
    )


def settings_to_dict(settings: HudSettings) -> dict[str, Any]:
    validate_settings(settings)
    return {
        "host": settings.host,
        "model": settings.model,
        "trigger_shortcut": settings.trigger_shortcut,
        "exit_shortcut": settings.exit_shortcut,
        "clear_shortcut": settings.clear_shortcut,
        "screenshot_max_edge": settings.screenshot_max_edge,
        "timeout_seconds": settings.timeout_seconds,
        "memory_qa_pairs": settings.memory_qa_pairs,
        "instruction": settings.instruction,
        "keep_alive": settings.keep_alive,
        "think": settings.think,
        "query": settings.query,
        "options": dict(settings.options),
    }


def validate_settings(settings: HudSettings) -> None:
    if not settings.host.strip():
        raise ValueError("Ollama host is required.")
    if not settings.model.strip():
        raise ValueError("Model is required.")
    parse_shortcut(settings.trigger_shortcut)
    parse_shortcut(settings.exit_shortcut)
    parse_shortcut(settings.clear_shortcut)
    if settings.screenshot_max_edge < 64:
        raise ValueError("Screenshot max edge must be at least 64.")
    if settings.timeout_seconds < 1:
        raise ValueError("Timeout seconds must be at least 1.")
    if settings.memory_qa_pairs < 0 or settings.memory_qa_pairs > 20:
        raise ValueError("Q/A memory pairs must be between 0 and 20.")
    if not settings.instruction.strip():
        raise ValueError("Instruction is required.")
    if not settings.query.strip():
        raise ValueError("Query is required.")
    if not settings.keep_alive.strip():
        raise ValueError("Keep alive is required.")


def _preferred_config_path() -> Path:
    return USER_CONFIG_PATH if USER_CONFIG_PATH.exists() else DEFAULT_CONFIG_PATH


def _read_yaml_dict(path: Path) -> dict[str, Any]:
    try:
        data = yaml.safe_load(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {}
    if not isinstance(data, dict):
        return {}
    return data


def _string(value: object, default: str) -> str:
    return value if isinstance(value, str) and value.strip() else default


def _int(value: object, default: int) -> int:
    if isinstance(value, bool):
        return default
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value)
        except ValueError:
            return default
    return default


def _float(value: object, default: float) -> float:
    if isinstance(value, bool):
        return default
    if isinstance(value, int | float):
        return float(value)
    if isinstance(value, str):
        try:
            return float(value)
        except ValueError:
            return default
    return default


def _bool(value: object, default: bool) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"true", "yes", "on", "1"}:
            return True
        if normalized in {"false", "no", "off", "0"}:
            return False
    return default
