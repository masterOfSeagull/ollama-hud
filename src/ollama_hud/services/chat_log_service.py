from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

from ollama_hud.services.ollama_service import (
    ChatMemory,
    build_message_preview,
    select_prompt_memories,
)
from ollama_hud.services.settings_service import CHAT_LOG_PATH, HudSettings


@dataclass(frozen=True)
class ChatLogEntry:
    capture_id: str
    question: str
    memories: tuple[ChatMemory, ...]
    answer: str | None = None
    error: str | None = None
    retry: str = "none"
    thinking: str | None = None


class ChatLogger:
    def __init__(self, path: str | Path = CHAT_LOG_PATH):
        self.path = Path(path)

    def write(self, entry: ChatLogEntry, settings: HudSettings) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("a", encoding="utf-8") as handle:
            handle.write(_format_entry(entry, settings))


def _format_entry(entry: ChatLogEntry, settings: HudSettings) -> str:
    sent_memories = select_prompt_memories(entry.memories, settings.memory_qa_pairs)
    lines = [
        "=" * 80,
        f"Timestamp: {datetime.now().astimezone().isoformat(timespec='seconds')}",
        f"Model: {settings.model}",
        f"Host: {settings.host}",
        f"Capture ID: {entry.capture_id}",
        "Screenshot attached: yes; payload omitted from this text log",
        f"Screenshot max edge: {settings.screenshot_max_edge}",
        f"Q/A memory configured: {settings.memory_qa_pairs}",
        f"Q/A memory available: {len(entry.memories)}",
        f"Q/A memory sent: {len(sent_memories)}",
        f"Think: {'yes' if settings.think else 'no'}",
        f"Retry: {entry.retry}",
        "",
        "Question:",
        entry.question,
        "",
        "Included Memory:",
    ]
    if sent_memories:
        for index, item in enumerate(sent_memories, start=1):
            lines.extend(
                [
                    f"{index}. Q: {_one_line(item.question)}",
                    f"   A: {_one_line(item.answer)}",
                ]
            )
    else:
        lines.append("(none)")

    lines.extend(["", "Answer:"])
    lines.append(entry.answer if entry.answer is not None else "(none)")
    lines.extend(["", "Thinking:"])
    lines.append(entry.thinking if entry.thinking is not None else "(none)")
    if entry.error is not None:
        lines.extend(["", "Error:", entry.error])
    lines.extend(
        [
            "",
            "Message Preview Sent:",
            build_message_preview(
                entry.question,
                instruction=settings.instruction,
                memories=sent_memories,
                max_memories=settings.memory_qa_pairs,
            ),
        ]
    )
    lines.append("")
    return "\n".join(lines) + "\n"


def _one_line(text: str) -> str:
    return " ".join(part.strip() for part in text.splitlines() if part.strip()).strip()
