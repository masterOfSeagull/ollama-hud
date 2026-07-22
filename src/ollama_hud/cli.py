from __future__ import annotations

import sys

from ollama_hud.native_launcher import delegate


def main(argv: list[str] | None = None) -> int:
    return delegate(argv if argv is not None else sys.argv[1:])


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
