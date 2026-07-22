from __future__ import annotations

import queue
import threading
import time
from collections import deque
from collections.abc import Callable
from contextlib import suppress
from dataclasses import dataclass
from typing import Protocol

from PIL import Image

from ollama_hud.core.state import HudState, RuntimeSnapshot
from ollama_hud.services.capture_service import (
    capture_primary_monitor,
    encode_jpeg_base64,
    image_fingerprint,
)
from ollama_hud.services.chat_log_service import ChatLogEntry, ChatLogger
from ollama_hud.services.hotkey_service import (
    ShortcutLatch,
    exit_shortcut_pressed,
    parse_shortcut,
)
from ollama_hud.services.ollama_service import (
    ChatMemory,
    OllamaClient,
    OllamaError,
    is_context_limit_error,
)
from ollama_hud.services.settings_service import HudSettings

CaptureFunc = Callable[[], Image.Image]


class OverlayHandle(Protocol):
    closed: bool

    def display(self, title: str, message: str = "", *, error: bool = False) -> bool: ...

    def hide(self) -> None: ...

    def show(self) -> None: ...

    def poll(self) -> bool: ...

    def close(self) -> None: ...


@dataclass(frozen=True)
class HudServices:
    capture_func: CaptureFunc = capture_primary_monitor
    client: OllamaClient | None = None
    chat_logger: ChatLogger | None = None


class HudController:
    def __init__(
        self,
        settings: HudSettings,
        *,
        services: HudServices | None = None,
        capture_func: CaptureFunc = capture_primary_monitor,
        client: OllamaClient | None = None,
        chat_logger: ChatLogger | None = None,
        capture_delay_seconds: float = 0.08,
    ) -> None:
        self.settings = settings
        service_bundle = services or HudServices()
        self.capture_func = (
            capture_func
            if capture_func is not capture_primary_monitor
            else service_bundle.capture_func
        )
        self.client = client or service_bundle.client or OllamaClient(settings)
        self.chat_logger = chat_logger or service_bundle.chat_logger or ChatLogger()
        self.capture_delay_seconds = capture_delay_seconds
        self.trigger_shortcut = parse_shortcut(settings.trigger_shortcut)
        self.exit_shortcut = parse_shortcut(settings.exit_shortcut)
        self.clear_shortcut = parse_shortcut(settings.clear_shortcut)
        self.trigger_latch = ShortcutLatch(self.trigger_shortcut)
        self.clear_latch = ShortcutLatch(self.clear_shortcut)
        self._updates: queue.Queue[RuntimeSnapshot] = queue.Queue()
        self._snapshot = RuntimeSnapshot(
            HudState.READY,
            f"Ready - press {self.trigger_shortcut.display}",
            active=False,
        )
        self._active = False
        self._stopped = False
        self._worker: threading.Thread | None = None
        self._memory: deque[ChatMemory] = deque(maxlen=settings.memory_qa_pairs)
        self._memory_lock = threading.Lock()

    @property
    def snapshot(self) -> RuntimeSnapshot:
        return self._snapshot

    @property
    def active(self) -> bool:
        return self._active

    @property
    def memories(self) -> tuple[ChatMemory, ...]:
        with self._memory_lock:
            return tuple(self._memory)

    def clear_memory(self) -> None:
        with self._memory_lock:
            self._memory.clear()

    def should_exit(self) -> bool:
        return exit_shortcut_pressed(self.exit_shortcut)

    def trigger_pressed_once(self) -> bool:
        return self.trigger_latch.consume_press()

    def clear_pressed_once(self) -> bool:
        return self.clear_latch.consume_press()

    def clear_visual_answer(self) -> RuntimeSnapshot:
        snapshot = RuntimeSnapshot(
            HudState.READY,
            f"Ready - press {self.trigger_shortcut.display}",
            active=self._active,
        )
        self._set_snapshot(snapshot)
        return snapshot

    def poll_updates(self) -> RuntimeSnapshot:
        while True:
            try:
                self._snapshot = self._updates.get_nowait()
            except queue.Empty:
                return self._snapshot

    def start_request(self, overlay: OverlayHandle | None = None) -> bool:
        if self._active:
            return False
        self._active = True
        self._set_snapshot(RuntimeSnapshot(HudState.CAPTURING, "Capturing primary monitor.", True))
        image = self._capture_with_overlay_hidden(overlay)
        if image is None:
            self._active = False
            return False
        capture_id = image_fingerprint(image)
        self._set_snapshot(
            RuntimeSnapshot(
                HudState.ASKING,
                f"Asking Ollama. Capture {capture_id}.",
                True,
                capture_id,
            )
        )
        self._worker = threading.Thread(
            target=self._ask_worker,
            args=(image, capture_id),
            daemon=True,
        )
        self._worker.start()
        return True

    def run_once_sync(self) -> RuntimeSnapshot:
        if self._active:
            return self._snapshot
        self._active = True
        self._set_snapshot(RuntimeSnapshot(HudState.CAPTURING, "Capturing primary monitor.", True))
        try:
            image = self.capture_func()
        except Exception as exc:
            self._active = False
            self._set_snapshot(RuntimeSnapshot(HudState.ERROR, _short_error(exc), False))
            return self._snapshot
        capture_id = image_fingerprint(image)
        self._set_snapshot(
            RuntimeSnapshot(
                HudState.ASKING,
                f"Asking Ollama. Capture {capture_id}.",
                True,
                capture_id,
            )
        )
        self._finish_with_image(image, capture_id)
        return self._snapshot

    def wait_for_worker(self, timeout: float | None = None) -> RuntimeSnapshot:
        if self._worker is not None:
            self._worker.join(timeout=timeout)
        return self.poll_updates()

    def start_overlay_loop(self) -> int:
        from ollama_hud.ui.status_overlay import StatusHud

        overlay = StatusHud()
        try:
            overlay.display(self.snapshot.state.value, self.snapshot.message)
            while not self._stopped and overlay.poll():
                snapshot = self.poll_updates()
                overlay.display(snapshot.state.value, snapshot.message, error=snapshot.is_error)
                if self.should_exit():
                    break
                if self.clear_pressed_once() and not self.active:
                    self.clear_visual_answer()
                if self.trigger_pressed_once() and not self.active:
                    self.start_request(overlay)
                time.sleep(0.03)
        finally:
            overlay.close()
            self.stop()
        return 0

    def stop(self) -> None:
        self._stopped = True

    def _capture_with_overlay_hidden(self, overlay: OverlayHandle | None) -> Image.Image | None:
        try:
            if overlay is not None:
                overlay.hide()
                time.sleep(self.capture_delay_seconds)
            image = self.capture_func()
        except Exception as exc:
            self._set_snapshot(RuntimeSnapshot(HudState.ERROR, _short_error(exc), False))
            return None
        finally:
            if overlay is not None:
                overlay.show()
        return image

    def _ask_worker(self, image: Image.Image, capture_id: str) -> None:
        self._finish_with_image(image, capture_id)

    def _finish_with_image(self, image: Image.Image, capture_id: str) -> None:
        memories = self.memories
        retry = "none"
        thinking: str | None = None
        try:
            answer, retry, thinking = self._ask_ollama_with_retry(image, memories)
        except Exception as exc:
            error = _short_error(exc)
            if isinstance(exc, OllamaError):
                thinking = exc.thinking or self.client.last_thinking
            else:
                thinking = self.client.last_thinking
            self._write_chat_log(
                ChatLogEntry(
                    capture_id=capture_id,
                    question=self.settings.query,
                    memories=memories,
                    error=error,
                    retry=retry,
                    thinking=thinking,
                )
            )
            snapshot = RuntimeSnapshot(HudState.ERROR, error, False, capture_id)
        else:
            self._remember_answer(answer, image)
            self._write_chat_log(
                ChatLogEntry(
                    capture_id=capture_id,
                    question=self.settings.query,
                    memories=memories,
                    answer=answer,
                    retry=retry,
                    thinking=thinking,
                )
            )
            snapshot = RuntimeSnapshot(HudState.ANSWER, answer, False, capture_id)
        self._active = False
        self._set_snapshot(snapshot)

    def _ask_ollama_with_retry(
        self,
        image: Image.Image,
        memories: tuple[ChatMemory, ...],
    ) -> tuple[str, str, str | None]:
        initial = encode_jpeg_base64(
            image,
            max_edge=self.settings.screenshot_max_edge,
            quality=85,
        )
        try:
            answer = self.client.generate_from_image(initial, memories=memories)
            return answer, "none", self.client.last_thinking
        except OllamaError as exc:
            if not is_context_limit_error(exc):
                raise

        compact = encode_jpeg_base64(image, max_edge=768, quality=70)
        try:
            answer = self.client.generate_from_image(compact, memories=memories)
            return answer, "compact screenshot", self.client.last_thinking
        except OllamaError as exc:
            if is_context_limit_error(exc):
                raise OllamaError("Context too large; lower capture size.") from exc
            raise

    def _remember_answer(self, answer: str, image: Image.Image) -> None:
        image_b64 = encode_jpeg_base64(image, max_edge=768, quality=70)
        with self._memory_lock:
            current = ChatMemory(
                question=self.settings.query,
                answer=answer,
                image_b64=image_b64,
            )
            if self._memory and (
                self._memory[-1].question == current.question
                and self._memory[-1].answer == current.answer
            ):
                return
            self._memory.append(current)

    def _write_chat_log(self, entry: ChatLogEntry) -> None:
        with suppress(OSError):
            self.chat_logger.write(entry, self.settings)

    def _set_snapshot(self, snapshot: RuntimeSnapshot) -> None:
        self._snapshot = snapshot
        self._updates.put(snapshot)


def run_hud(settings: HudSettings) -> int:
    return HudController(settings).start_overlay_loop()


def _short_error(error: BaseException | str, limit: int = 180) -> str:
    text = str(error).strip() or error.__class__.__name__
    if len(text) <= limit:
        return text
    return f"{text[: limit - 3]}..."


OllamaHudRuntime = HudController
