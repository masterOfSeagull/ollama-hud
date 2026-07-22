from __future__ import annotations

import base64
import hashlib
import io

from PIL import Image


def capture_primary_monitor() -> Image.Image:
    try:
        import dxcam
    except ModuleNotFoundError as exc:
        raise RuntimeError("Primary monitor capture requires dxcam on Windows.") from exc

    camera = dxcam.create(output_idx=0)
    if camera is None:
        raise RuntimeError("Could not open primary monitor with dxcam.")
    try:
        frame = camera.grab()
    finally:
        camera.release()
    if frame is None:
        raise RuntimeError("dxcam did not return a frame.")
    return Image.fromarray(frame).convert("RGB")


def resize_preserving_aspect(image: Image.Image, max_edge: int) -> Image.Image:
    if max_edge <= 0:
        raise ValueError("max_edge must be positive")
    width, height = image.size
    longest = max(width, height)
    if longest <= max_edge:
        return image.convert("RGB")
    scale = max_edge / longest
    size = (max(1, round(width * scale)), max(1, round(height * scale)))
    return image.convert("RGB").resize(size, Image.Resampling.LANCZOS)


def encode_jpeg_base64(image: Image.Image, *, max_edge: int, quality: int) -> str:
    resized = resize_preserving_aspect(image, max_edge)
    buffer = io.BytesIO()
    resized.save(buffer, format="JPEG", quality=quality, optimize=True)
    return base64.b64encode(buffer.getvalue()).decode("ascii")


def image_fingerprint(image: Image.Image) -> str:
    rgb = image.convert("RGB")
    digest = hashlib.sha256(rgb.tobytes()).hexdigest()
    return digest[:10]
