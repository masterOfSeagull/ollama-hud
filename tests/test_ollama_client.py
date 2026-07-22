from __future__ import annotations

from ollama_hud.services.ollama_service import (
    CONCISE_INSTRUCTION,
    SCREENSHOT_CONTEXT,
    ChatMemory,
    build_chat_payload,
    is_context_limit_error,
    select_prompt_memories,
)
from ollama_hud.services.settings_service import HudSettings


def test_payload_builder_uses_expected_options_and_no_ollama_context_field():
    settings = HudSettings()

    payload = build_chat_payload(settings=settings, image_b64="abc123")

    assert payload["model"] == settings.model
    assert payload["stream"] is False
    assert payload["think"] is True
    assert payload["keep_alive"] == "30m"
    assert payload["options"]["temperature"] == 0.2
    assert payload["options"]["top_p"] == 0.8
    assert payload["options"]["num_predict"] == 2048
    assert payload["options"]["num_ctx"] == 32768
    assert payload["options"]["repeat_penalty"] == 1.1
    assert payload["options"]["repeat_last_n"] == 64
    assert "context" not in payload
    assert "prompt" not in payload
    assert "images" not in payload
    assert payload["messages"] == [
        {"role": "system", "content": CONCISE_INSTRUCTION},
        {"role": "system", "content": SCREENSHOT_CONTEXT},
        {
            "role": "user",
            "content": settings.query,
            "images": ["abc123"],
        },
    ]


def test_payload_builder_uses_configured_instruction():
    settings = HudSettings(
        instruction="Give a detailed explanation with visible evidence.",
        query="What changed?",
    )

    payload = build_chat_payload(settings=settings, image_b64="abc123")

    messages = payload["messages"]
    assert messages[0] == {
        "role": "system",
        "content": "Give a detailed explanation with visible evidence.",
    }
    assert CONCISE_INSTRUCTION not in {message["content"] for message in messages}


def test_payload_builder_includes_recent_three_qa_memories_with_screenshots():
    settings = HudSettings(query="Where now?")
    memories = (
        ChatMemory("Q1", "A1", "img1"),
        ChatMemory("Q2", "A2", "img2"),
        ChatMemory("Q3", "A3", "img3"),
        ChatMemory("Q4", "A4", "img4"),
    )

    payload = build_chat_payload(
        settings=settings,
        image_b64="abc123",
        memories=memories,
    )

    messages = payload["messages"]
    assert [message["role"] for message in messages] == [
        "system",
        "system",
        "user",
        "assistant",
        "user",
        "assistant",
        "user",
        "assistant",
        "user",
    ]
    assert "source of truth" in messages[1]["content"]
    assert [message["content"] for message in messages if message["role"] == "user"] == [
        "Q2",
        "Q3",
        "Q4",
        "Where now?",
    ]
    assert [message["content"] for message in messages if message["role"] == "assistant"] == [
        "A2",
        "A3",
        "A4",
    ]
    assert [message["images"] for message in messages if message["role"] == "user"] == [
        ["img2"],
        ["img3"],
        ["img4"],
        ["abc123"],
    ]


def test_prompt_memory_deduplicates_repeated_qa_pairs():
    memories = (
        ChatMemory("Where now?", "Go left.", "img1"),
        ChatMemory("Where now?", "Go left.", "img2"),
        ChatMemory("Where now?", "Go right.", "img3"),
    )

    selected = select_prompt_memories(memories, max_memories=3)

    assert selected == (
        ChatMemory("Where now?", "Go left.", "img2"),
        ChatMemory("Where now?", "Go right.", "img3"),
    )


def test_payload_builder_uses_configured_memory_count():
    settings = HudSettings(query="Where now?", memory_qa_pairs=1)
    memories = (
        ChatMemory("Q1", "A1", "img1"),
        ChatMemory("Q2", "A2", "img2"),
    )

    payload = build_chat_payload(
        settings=settings,
        image_b64="abc123",
        memories=memories,
    )

    messages = payload["messages"]
    assert [message["content"] for message in messages] == [
        CONCISE_INSTRUCTION,
        SCREENSHOT_CONTEXT,
        "Q2",
        "A2",
        "Where now?",
    ]
    assert [message["images"] for message in messages if message["role"] == "user"] == [
        ["img2"],
        ["abc123"],
    ]


def test_payload_builder_omits_memory_when_count_is_zero():
    settings = HudSettings(query="Where now?", memory_qa_pairs=0)

    payload = build_chat_payload(
        settings=settings,
        image_b64="abc123",
        memories=(ChatMemory("Q1", "A1", "img1"),),
    )

    assert payload["messages"] == [
        {"role": "system", "content": CONCISE_INSTRUCTION},
        {"role": "system", "content": SCREENSHOT_CONTEXT},
        {"role": "user", "content": "Where now?", "images": ["abc123"]},
    ]


def test_context_error_classifier_detects_common_payload_and_context_errors():
    assert is_context_limit_error("context length exceeded")
    assert is_context_limit_error("request entity too large")
    assert is_context_limit_error("HTTP 413")
    assert not is_context_limit_error("connection refused")


def test_payload_builder_uses_configured_thinking_toggle_and_repeat_options():
    settings = HudSettings(
        think=False,
        options={
            "temperature": 0.1,
            "top_p": 0.7,
            "num_predict": 4096,
            "num_ctx": 32768,
            "repeat_penalty": 1.25,
            "repeat_last_n": 256,
        },
    )

    payload = build_chat_payload(settings=settings, image_b64="abc123")

    assert payload["think"] is False
    assert payload["options"]["repeat_penalty"] == 1.25
    assert payload["options"]["repeat_last_n"] == 256
