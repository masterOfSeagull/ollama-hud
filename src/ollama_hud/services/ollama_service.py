from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import requests

from ollama_hud.services.settings_service import DEFAULT_INSTRUCTION, HudSettings

CONCISE_INSTRUCTION = DEFAULT_INSTRUCTION
SCREENSHOT_CONTEXT = (
    "Prior screenshots and Q&A turns are stale context. Use the current screenshot as the "
    "source of truth, and use prior turns only when the current screenshot supports them."
)

CONTEXT_ERROR_MARKERS = (
    "context length",
    "context window",
    "context is full",
    "maximum context",
    "num_ctx",
    "too many tokens",
    "input is too long",
    "payload too large",
    "request entity too large",
    "image too large",
    "413",
)


@dataclass(frozen=True)
class ChatMemory:
    question: str
    answer: str
    image_b64: str


@dataclass
class OllamaError(Exception):
    message: str
    status_code: int | None = None
    thinking: str | None = None

    def __str__(self) -> str:
        return self.message


def build_chat_payload(
    *,
    settings: HudSettings,
    image_b64: str,
    query: str | None = None,
    memories: tuple[ChatMemory, ...] = (),
    max_memories: int | None = None,
) -> dict[str, Any]:
    memory_limit = settings.memory_qa_pairs if max_memories is None else max_memories
    messages = build_chat_messages(
        query if query is not None else settings.query,
        instruction=settings.instruction,
        image_b64=image_b64,
        memories=memories,
        max_memories=memory_limit,
    )
    return {
        "model": settings.model,
        "messages": messages,
        "stream": False,
        "think": settings.think,
        "keep_alive": settings.keep_alive,
        "options": dict(settings.options),
    }


def build_chat_messages(
    query: str,
    *,
    instruction: str = CONCISE_INSTRUCTION,
    image_b64: str,
    memories: tuple[ChatMemory, ...] = (),
    max_memories: int = 3,
) -> list[dict[str, Any]]:
    messages: list[dict[str, Any]] = [
        {"role": "system", "content": instruction},
        {"role": "system", "content": SCREENSHOT_CONTEXT},
    ]
    recent = select_prompt_memories(memories, max_memories)
    for item in recent:
        messages.append(
            {
                "role": "user",
                "content": _compact(item.question),
                "images": [item.image_b64],
            }
        )
        messages.append({"role": "assistant", "content": _compact(item.answer)})
    messages.append({"role": "user", "content": query, "images": [image_b64]})
    return messages


def build_message_preview(
    query: str,
    *,
    instruction: str = CONCISE_INSTRUCTION,
    memories: tuple[ChatMemory, ...] = (),
    max_memories: int = 3,
) -> str:
    parts = [
        f"system: {instruction}",
        f"system: {SCREENSHOT_CONTEXT}",
    ]
    for item in select_prompt_memories(memories, max_memories):
        parts.append(f"user: {_compact(item.question)} [screenshot omitted]")
        parts.append(f"assistant: {_compact(item.answer)}")
    parts.append(f"user: {query} [screenshot omitted]")
    return "\n".join(parts)


def select_prompt_memories(
    memories: tuple[ChatMemory, ...],
    max_memories: int,
) -> tuple[ChatMemory, ...]:
    if max_memories <= 0:
        return ()
    selected: list[ChatMemory] = []
    seen: set[tuple[str, str]] = set()
    for item in reversed(memories):
        key = (_one_line(item.question).lower(), _one_line(item.answer).lower())
        if key in seen:
            continue
        seen.add(key)
        selected.append(item)
        if len(selected) >= max_memories:
            break
    return tuple(reversed(selected))


def is_context_limit_error(error: BaseException | str) -> bool:
    text = str(error).lower()
    return any(marker in text for marker in CONTEXT_ERROR_MARKERS)


class OllamaClient:
    def __init__(self, settings: HudSettings):
        self.settings = settings
        self.last_thinking: str | None = None

    def generate_from_image(
        self,
        image_b64: str,
        *,
        memories: tuple[ChatMemory, ...] = (),
    ) -> str:
        payload = build_chat_payload(
            settings=self.settings,
            image_b64=image_b64,
            memories=memories,
        )
        data = self._post_chat(payload)
        message = data.get("message")
        if not isinstance(message, dict):
            raise OllamaError("Ollama did not return a chat message.")
        thinking = _optional_one_line(message.get("thinking"))
        self.last_thinking = thinking
        response = message.get("content")
        if not isinstance(response, str) or not response.strip():
            raise OllamaError("Ollama returned an empty response.", thinking=thinking)
        return _one_line(response)

    def test_model(self) -> str:
        payload = {
            "model": self.settings.model,
            "messages": [
                {"role": "system", "content": self.settings.instruction},
                {"role": "user", "content": "Reply with OK."},
            ],
            "stream": False,
            "think": self.settings.think,
            "keep_alive": self.settings.keep_alive,
            "options": {**dict(self.settings.options), "temperature": 0},
        }
        data = self._post_chat(payload)
        message = data.get("message")
        if not isinstance(message, dict):
            raise OllamaError("Ollama did not return a chat message.")
        self.last_thinking = _optional_one_line(message.get("thinking"))
        response = message.get("content")
        if not isinstance(response, str):
            raise OllamaError("Ollama did not return a text response.")
        return _one_line(response) or "OK"

    def check_server(self) -> str:
        url = f"{self.settings.normalized_host()}/api/tags"
        try:
            response = requests.get(url, timeout=min(self.settings.timeout_seconds, 10))
        except requests.RequestException as exc:
            raise OllamaError(f"Could not reach Ollama at {self.settings.host}: {exc}") from exc
        if response.status_code >= 400:
            raise OllamaError(
                f"Ollama returned HTTP {response.status_code} from /api/tags.",
                response.status_code,
            )
        return "Ollama server is reachable."

    def _post_chat(self, payload: dict[str, Any]) -> dict[str, Any]:
        url = f"{self.settings.normalized_host()}/api/chat"
        try:
            response = requests.post(
                url,
                json=payload,
                timeout=self.settings.timeout_seconds,
            )
        except requests.RequestException as exc:
            raise OllamaError(f"Could not reach Ollama at {self.settings.host}: {exc}") from exc
        if response.status_code >= 400:
            raise OllamaError(_error_text(response), response.status_code)
        try:
            data = response.json()
        except ValueError as exc:
            raise OllamaError("Ollama returned invalid JSON.") from exc
        if not isinstance(data, dict):
            raise OllamaError("Ollama returned an unexpected response.")
        if "error" in data:
            raise OllamaError(str(data["error"]))
        return data


def _error_text(response: requests.Response) -> str:
    try:
        data = response.json()
    except ValueError:
        return response.text or f"HTTP {response.status_code}"
    if isinstance(data, dict) and "error" in data:
        return str(data["error"])
    return response.text or f"HTTP {response.status_code}"


def _one_line(text: str) -> str:
    return " ".join(part.strip() for part in text.splitlines() if part.strip()).strip()


def _optional_one_line(value: object) -> str | None:
    if not isinstance(value, str):
        return None
    text = _one_line(value)
    return text or None


def _compact(text: str, limit: int = 220) -> str:
    compact = _one_line(text)
    if len(compact) <= limit:
        return compact
    return f"{compact[: limit - 3]}..."
