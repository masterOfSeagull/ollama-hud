from __future__ import annotations

from PIL import Image

from ollama_hud.core.controller import HudController, HudServices
from ollama_hud.core.state import HudState
from ollama_hud.services.chat_log_service import ChatLogEntry
from ollama_hud.services.ollama_service import ChatMemory
from ollama_hud.services.settings_service import HudSettings


class FakeClient:
    def __init__(self) -> None:
        self.last_thinking = "fake thinking"
        self.calls: list[tuple[str, tuple[ChatMemory, ...]]] = []

    def generate_from_image(
        self,
        image_b64: str,
        *,
        memories: tuple[ChatMemory, ...] = (),
    ) -> str:
        self.calls.append((image_b64, memories))
        return "Use the north door."


class FakeLogger:
    def __init__(self) -> None:
        self.entries: list[ChatLogEntry] = []

    def write(self, entry: ChatLogEntry, settings: HudSettings) -> None:
        self.entries.append(entry)


def test_controller_runs_with_injected_service_fakes():
    client = FakeClient()
    logger = FakeLogger()
    controller = HudController(
        HudSettings(),
        services=HudServices(
            capture_func=lambda: Image.new("RGB", (32, 16), "green"),
            client=client,
            chat_logger=logger,
        ),
    )

    snapshot = controller.run_once_sync()

    assert snapshot.state is HudState.ANSWER
    assert snapshot.message == "Use the north door."
    assert len(client.calls) == 1
    assert logger.entries[0].answer == "Use the north door."
    assert controller.memories[0].answer == "Use the north door."


def test_cli_verify_delegates_to_native(monkeypatch):
    from ollama_hud import cli

    calls: list[list[str]] = []

    def fake_delegate(argv: list[str]) -> int:
        calls.append(argv)
        return 7

    monkeypatch.setattr(cli, "delegate", fake_delegate)

    assert cli.main(["verify"]) == 7
    assert calls == [["verify"]]


def test_native_launcher_translates_legacy_commands(monkeypatch, tmp_path):
    from ollama_hud import native_launcher

    exe = tmp_path / "OllamaHud.exe"
    exe.write_text("", encoding="utf-8")
    calls: list[list[str]] = []

    class Completed:
        returncode = 0

    def fake_run(command: list[str], check: bool = False, **kwargs: object) -> Completed:
        calls.append(command)
        return Completed()

    monkeypatch.setattr(native_launcher, "native_executable_candidates", lambda: (exe,))
    monkeypatch.setattr(native_launcher.subprocess, "run", fake_run)

    assert native_launcher.delegate(["verify"]) == 0
    assert native_launcher.delegate(["run"]) == 0
    assert calls == [[str(exe), "--verify"], [str(exe), "--run"]]
