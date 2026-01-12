#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def load_symbols(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    symbols: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        symbols.append(stripped)
    return symbols


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--file",
        default=None,
        help="Path to exported-functions.txt (defaults to scripts/exported-functions.txt next to this script).",
    )
    parser.add_argument(
        "--format",
        choices=("make", "lines"),
        default="lines",
        help="Output format: 'make' for comma-separated, 'lines' for newline-separated.",
    )
    args = parser.parse_args()

    symbols_file = Path(args.file) if args.file else Path(__file__).with_name("exported-functions.txt")
    symbols = load_symbols(symbols_file)

    if args.format == "make":
        print(",".join(symbols), end="")
        return 0

    print("\n".join(symbols))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
