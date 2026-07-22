from __future__ import annotations

import base64

from PIL import Image

from ollama_hud.services.capture_service import (
    encode_jpeg_base64,
    image_fingerprint,
    resize_preserving_aspect,
)


def test_resize_preserves_aspect_ratio_and_respects_max_edge():
    image = Image.new("RGB", (2000, 1000), "red")

    resized = resize_preserving_aspect(image, 1280)

    assert resized.size == (1280, 640)


def test_encode_jpeg_base64_returns_jpeg_bytes():
    image = Image.new("RGB", (24, 12), "blue")

    encoded = encode_jpeg_base64(image, max_edge=12, quality=70)
    data = base64.b64decode(encoded)

    assert data.startswith(b"\xff\xd8")


def test_image_fingerprint_changes_when_pixels_change():
    first = Image.new("RGB", (8, 8), "blue")
    second = Image.new("RGB", (8, 8), "red")

    assert image_fingerprint(first) != image_fingerprint(second)
    assert image_fingerprint(first) == image_fingerprint(first.copy())
