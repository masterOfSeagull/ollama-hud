from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class HudState(str, Enum):
    READY = "Ready"
    CAPTURING = "Capturing"
    ASKING = "Asking Ollama"
    ANSWER = "Answer"
    ERROR = "Error"


@dataclass(frozen=True)
class RuntimeSnapshot:
    state: HudState
    message: str = ""
    active: bool = False
    capture_id: str = ""

    @property
    def is_error(self) -> bool:
        return self.state is HudState.ERROR
