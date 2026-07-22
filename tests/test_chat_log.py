from __future__ import annotations

from ollama_hud.services.chat_log_service import ChatLogEntry, ChatLogger
from ollama_hud.services.ollama_service import ChatMemory
from ollama_hud.services.settings_service import HudSettings


def test_chat_logger_writes_text_only_inspection_log(tmp_path):
    path = tmp_path / "chat.log"
    logger = ChatLogger(path)

    logger.write(
        ChatLogEntry(
            capture_id="deadbeef00",
            question="Where should I go?",
            memories=(ChatMemory("Previous?", "Go left.", "prior-b64"),),
            answer="Go through the blue portal.",
            retry="none",
            thinking="The blue portal appears to be the only passable exit.",
        ),
        HudSettings(
            model="vision:test",
            host="http://localhost:11434",
            instruction="Explain the visible route choice.",
        ),
    )

    text = path.read_text(encoding="utf-8")
    assert "Timestamp:" in text
    assert "Model: vision:test" in text
    assert "Capture ID: deadbeef00" in text
    assert "Screenshot attached: yes" in text
    assert "Q/A memory sent: 1" in text
    assert "Question:" in text
    assert "Where should I go?" in text
    assert "Included Memory:" in text
    assert "Previous?" in text
    assert "Go left." in text
    assert "Answer:" in text
    assert "Go through the blue portal." in text
    assert "Thinking:" in text
    assert "The blue portal appears to be the only passable exit." in text
    assert "Message Preview Sent:" in text
    assert "Explain the visible route choice." in text
    assert "prior-b64" not in text
    assert '"images"' not in text.lower()
    assert "base64" not in text.lower()
