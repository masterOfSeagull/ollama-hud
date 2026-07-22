from __future__ import annotations

import json
import socket
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any

from PIL import Image

from ollama_hud.core.controller import HudController
from ollama_hud.core.state import HudState
from ollama_hud.services.chat_log_service import ChatLogger
from ollama_hud.services.settings_service import HudSettings


class FakeOllama:
    def __init__(self, responses: list[tuple[int, dict[str, Any]]]):
        self.responses = responses
        self.received: list[dict[str, Any]] = []
        self.paths: list[str] = []
        self.server = HTTPServer(("127.0.0.1", 0), self._handler())
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    @property
    def url(self) -> str:
        host, port = self.server.server_address
        return f"http://{host}:{port}"

    def start(self) -> None:
        self.thread.start()

    def close(self) -> None:
        self.server.shutdown()
        self.thread.join(timeout=2)
        self.server.server_close()

    def _handler(self) -> type[BaseHTTPRequestHandler]:
        owner = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self) -> None:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                owner.paths.append(self.path)
                owner.received.append(json.loads(body.decode("utf-8")))
                status, payload = owner.responses.pop(0)
                data = json.dumps(payload).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, format: str, *args: object) -> None:
                return

        return Handler


def test_runtime_success_transitions_to_answer_state(tmp_path):
    fake = FakeOllama(
        [(200, {"message": {"role": "assistant", "content": "Go through the glowing door."}})]
    )
    fake.start()
    try:
        runtime = HudController(
            HudSettings(host=fake.url, timeout_seconds=3),
            capture_func=_capture,
            chat_logger=ChatLogger(tmp_path / "chat.log"),
        )

        snapshot = runtime.run_once_sync()

        assert snapshot.state is HudState.ANSWER
        assert snapshot.message == "Go through the glowing door."
        assert snapshot.capture_id
        assert runtime.active is False
        assert len(fake.received) == 1
        assert fake.paths == ["/api/chat"]
        assert fake.received[0]["think"] is True
        assert [message["role"] for message in fake.received[0]["messages"]] == [
            "system",
            "system",
            "user",
        ]
        log_text = (tmp_path / "chat.log").read_text(encoding="utf-8")
        assert "Question:" in log_text
        assert "Answer:" in log_text
        assert "Go through the glowing door." in log_text
        assert "Message Preview Sent:" in log_text
        assert "abc123" not in log_text
    finally:
        fake.close()


def test_runtime_context_error_retries_once_with_compact_image(tmp_path):
    fake = FakeOllama(
        [
            (200, {"message": {"role": "assistant", "content": "Use the first portal."}}),
            (500, {"error": "context length exceeded"}),
            (200, {"message": {"role": "assistant", "content": "Take the left portal."}}),
        ]
    )
    fake.start()
    try:
        runtime = HudController(
            HudSettings(host=fake.url, timeout_seconds=3),
            capture_func=_capture,
            chat_logger=ChatLogger(tmp_path / "chat.log"),
        )

        first_snapshot = runtime.run_once_sync()
        snapshot = runtime.run_once_sync()

        assert first_snapshot.state is HudState.ANSWER
        assert snapshot.state is HudState.ANSWER
        assert snapshot.message == "Take the left portal."
        assert len(fake.received) == 3
        first_current = fake.received[0]["messages"][-1]["images"][0]
        initial_messages = fake.received[1]["messages"]
        retry_messages = fake.received[2]["messages"]
        assert initial_messages[2]["images"][0] == retry_messages[2]["images"][0]
        assert initial_messages[2]["images"][0] == runtime.memories[0].image_b64
        assert len(initial_messages[2]["images"][0]) <= len(first_current)
        initial_current = initial_messages[-1]["images"][0]
        retry_current = retry_messages[-1]["images"][0]
        assert initial_current != retry_current
        assert first_current != retry_current
        assert "Retry: compact screenshot" in (tmp_path / "chat.log").read_text(
            encoding="utf-8"
        )
    finally:
        fake.close()


def test_runtime_logs_thinking_when_chat_content_is_empty(tmp_path):
    fake = FakeOllama(
        [
            (
                200,
                {
                    "message": {
                        "role": "assistant",
                        "thinking": "I can see a doorway shape, but no final answer was emitted.",
                        "content": "",
                    }
                },
            ),
        ]
    )
    fake.start()
    try:
        runtime = HudController(
            HudSettings(host=fake.url, timeout_seconds=3),
            capture_func=_capture,
            chat_logger=ChatLogger(tmp_path / "chat.log"),
        )

        snapshot = runtime.run_once_sync()

        assert snapshot.state is HudState.ERROR
        assert snapshot.message == "Ollama returned an empty response."
        log_text = (tmp_path / "chat.log").read_text(encoding="utf-8")
        assert "Thinking:" in log_text
        assert "I can see a doorway shape, but no final answer was emitted." in log_text
        assert "Error:" in log_text
    finally:
        fake.close()


def test_runtime_uses_configured_qa_memory_count(tmp_path):
    fake = FakeOllama(
        [
            (200, {"message": {"role": "assistant", "content": "Answer 1"}}),
            (200, {"message": {"role": "assistant", "content": "Answer 2"}}),
            (200, {"message": {"role": "assistant", "content": "Answer 3"}}),
            (200, {"message": {"role": "assistant", "content": "Answer 4"}}),
            (200, {"message": {"role": "assistant", "content": "Answer 5"}}),
        ]
    )
    fake.start()
    try:
        runtime = HudController(
            HudSettings(
                host=fake.url,
                timeout_seconds=3,
                query="Where is the door?",
                memory_qa_pairs=2,
            ),
            capture_func=_capture,
            chat_logger=ChatLogger(tmp_path / "chat.log"),
        )

        for _ in range(5):
            snapshot = runtime.run_once_sync()
            assert snapshot.state is HudState.ANSWER

        fifth_contents = [message["content"] for message in fake.received[4]["messages"]]
        assert "Answer 1" not in fifth_contents
        assert "Answer 2" not in fifth_contents
        assert "Answer 3" in fifth_contents
        assert "Answer 4" in fifth_contents
        fifth_user_images = [
            message["images"][0]
            for message in fake.received[4]["messages"]
            if message["role"] == "user"
        ]
        assert len(fifth_user_images) == 3
        assert len(set(fifth_user_images[:2])) == 1
        assert fifth_user_images[-1]
        assert len(runtime.memories) == 2
    finally:
        fake.close()


def test_runtime_does_not_append_identical_memory_twice(tmp_path):
    fake = FakeOllama(
        [
            (200, {"message": {"role": "assistant", "content": "Same answer"}}),
            (200, {"message": {"role": "assistant", "content": "Same answer"}}),
            (200, {"message": {"role": "assistant", "content": "Different answer"}}),
        ]
    )
    fake.start()
    try:
        runtime = HudController(
            HudSettings(host=fake.url, timeout_seconds=3, memory_qa_pairs=3),
            capture_func=_capture,
            chat_logger=ChatLogger(tmp_path / "chat.log"),
        )

        for _ in range(3):
            snapshot = runtime.run_once_sync()
            assert snapshot.state is HudState.ANSWER

        assert [memory.answer for memory in runtime.memories] == [
            "Same answer",
            "Different answer",
        ]
    finally:
        fake.close()


def test_runtime_clear_visual_answer_returns_to_ready_without_clearing_memory(tmp_path):
    fake = FakeOllama(
        [
            (200, {"message": {"role": "assistant", "content": "Same answer"}}),
        ]
    )
    fake.start()
    try:
        runtime = HudController(
            HudSettings(host=fake.url, timeout_seconds=3),
            capture_func=_capture,
            chat_logger=ChatLogger(tmp_path / "chat.log"),
        )

        answer = runtime.run_once_sync()
        cleared = runtime.clear_visual_answer()

        assert answer.state is HudState.ANSWER
        assert cleared.state is HudState.READY
        assert cleared.message == "Ready - press Alt+1"
        assert len(runtime.memories) == 1
    finally:
        fake.close()


def test_runtime_server_unavailable_sets_error_and_becomes_inactive(tmp_path):
    runtime = HudController(
        HudSettings(host=f"http://127.0.0.1:{_unused_port()}", timeout_seconds=1),
        capture_func=_capture,
        chat_logger=ChatLogger(tmp_path / "chat.log"),
    )

    snapshot = runtime.run_once_sync()

    assert snapshot.state is HudState.ERROR
    assert runtime.active is False
    assert "Could not reach Ollama" in snapshot.message
    log_text = (tmp_path / "chat.log").read_text(encoding="utf-8")
    assert "Error:" in log_text
    assert "Could not reach Ollama" in log_text


def _capture() -> Image.Image:
    return Image.new("RGB", (960, 540), "black")


def _unused_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])
