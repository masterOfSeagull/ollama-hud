from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD_SCRIPT = PROJECT_ROOT / "scripts" / "build_native.ps1"


def main(argv: list[str] | None = None) -> int:
    return delegate(argv if argv is not None else sys.argv[1:])


def delegate(argv: list[str]) -> int:
    exe = ensure_native_executable()
    completed = subprocess.run(
        [str(exe), *_translate_args(argv)],
        check=False,
        env=_native_env(exe),
    )
    return int(completed.returncode)


def ensure_native_executable(*, build: bool = True) -> Path:
    for candidate in native_executable_candidates():
        if candidate.exists():
            return candidate
    if not build:
        raise FileNotFoundError("OllamaHud.exe was not found.")
    if not BUILD_SCRIPT.exists():
        raise FileNotFoundError(f"Native build script was not found: {BUILD_SCRIPT}")
    completed = subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(BUILD_SCRIPT),
        ],
        cwd=PROJECT_ROOT,
        check=False,
    )
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)
    for candidate in native_executable_candidates():
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Native build completed but OllamaHud.exe was not found.")


def native_executable_candidates() -> tuple[Path, ...]:
    return (
        PROJECT_ROOT / "build" / "native" / "Release" / "OllamaHud.exe",
        PROJECT_ROOT / "build" / "native" / "RelWithDebInfo" / "OllamaHud.exe",
        PROJECT_ROOT / "build" / "native" / "Debug" / "OllamaHud.exe",
        PROJECT_ROOT / "build" / "native" / "OllamaHud.exe",
        PROJECT_ROOT / "dist" / "OllamaHud" / "OllamaHud.exe",
        PROJECT_ROOT / "OllamaHud.exe",
    )


def _native_env(exe: Path) -> dict[str, str]:
    env = os.environ.copy()
    path_parts = [str(exe.parent), *[str(path) for path in _qt_bin_candidates()]]
    env["PATH"] = os.pathsep.join([*path_parts, env.get("PATH", "")])
    return env


def _qt_bin_candidates() -> tuple[Path, ...]:
    roots = [Path("C:/Qt")]
    candidates: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        for version in sorted(root.iterdir(), reverse=True):
            if not version.is_dir():
                continue
            for kit in sorted(version.glob("msvc*_64"), reverse=True):
                qt_bin = kit / "bin"
                if (qt_bin / "Qt6Core.dll").exists():
                    candidates.append(qt_bin)
    return tuple(candidates)


def _translate_args(argv: list[str]) -> list[str]:
    translated: list[str] = []
    for arg in argv:
        if arg == "verify":
            translated.append("--verify")
        elif arg == "run":
            translated.append("--run")
        elif arg == "gui":
            continue
        else:
            translated.append(arg)
    return translated
