#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path
from types import SimpleNamespace
from typing import Any

from psx_exe import PSX_EXE_HEADER_SIZE, load_verified_executable
from workspace import WorkspaceError, require_workspace_root, resolve_within


class CodeHashesError(RuntimeError):
    pass


OUTPUT = "config/slus_01411/code_hashes.json"
FUNCTIONS = "config/slus_01411/functions.csv"


def parse_integer(value: str, description: str) -> int:
    try:
        return int(value, 0)
    except (TypeError, ValueError) as error:
        raise CodeHashesError(f"{description} is not an integer: {value}") from error


def function_rows(root: Path) -> list[tuple[int, int, str]]:
    """The retail functions with their sizes: (address, size, name)."""
    path = resolve_within(root, FUNCTIONS, must_exist=True)
    rows: list[tuple[int, int, str]] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        for number, row in enumerate(csv.DictReader(handle), start=2):
            address = parse_integer(row["address"], f"{path}:{number}: address")
            size = parse_integer(row["size"], f"{path}:{number}: size")
            if not size:
                continue
            rows.append((address, size, row["name"]))
    if not rows:
        raise CodeHashesError(f"{path}: no function with a size")
    return rows


def build(root: Path, image_path: Path | None = None) -> dict[str, Any]:
    """The retail code's hashes, one per function.

    Without `image_path` the retail executable the target manifest names is
    used, and its hash is checked. With one, that executable is hashed
    instead: the table then says what *that* image's code looks like, which
    is what a test of the rule needs."""
    if image_path is None:
        source, executable, header_bytes, header = load_verified_executable(root)
    else:
        source = resolve_within(root, image_path, must_exist=True)
        data = source.read_bytes()
        if data[:8] != b"PS-X EXE":
            raise CodeHashesError(f"{source}: not a PS-X executable")
        header = SimpleNamespace(
            load_address=int.from_bytes(data[0x18:0x1C], "little"),
            initial_pc=int.from_bytes(data[0x10:0x14], "little"),
        )
        executable = {"sha256": hashlib.sha256(data).hexdigest()}
    image = source.read_bytes()
    if hashlib.sha256(image).hexdigest() != str(executable["sha256"]):
        raise CodeHashesError(f"{source}: the executable changed while it was read")
    functions = []
    for address, size, name in function_rows(root):
        offset = PSX_EXE_HEADER_SIZE + (address - header.load_address)
        if address < header.load_address or offset + size > len(image):
            raise CodeHashesError(f"{name} at {address:#010x} is outside the executable")
        functions.append({
            "address": f"0x{address:08X}",
            "size": f"0x{size:X}",
            "name": name,
            "sha256": hashlib.sha256(image[offset:offset + size]).hexdigest(),
        })
    return {
        "schema": 1,
        "target_sha256": str(executable["sha256"]),
        "functions": functions,
    }


def write(root: Path, table: dict[str, Any]) -> Path:
    path = resolve_within(root, OUTPUT)
    text = json.dumps(table, indent=2, sort_keys=False) + "\n"
    path.write_text(text, encoding="utf-8")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Write the retail code's hashes (config/slus_01411/code_hashes.json), "
                    "so that an image which changed a function can be told apart from one "
                    "that did not. Needs the retail executable (game/SLUS_014.11)."
    )
    parser.add_argument("--image", default=None,
                        help="hash this executable instead of the retail one "
                             "(for a test of the rule; the table then describes that image)")
    parser.add_argument("--check", action="store_true",
                        help="only say whether the table is up to date")
    arguments = parser.parse_args()
    try:
        # With --image the retail disc is not needed: the table describes
        # the image given, and the repository is only where the function
        # list and the output live.
        root = Path.cwd().resolve() if arguments.image else require_workspace_root()
        if arguments.image and not (root / FUNCTIONS).exists():
            raise WorkspaceError(f"run this command from the repository root; missing {FUNCTIONS}")
        table = build(root, arguments.image)
        path = resolve_within(root, OUTPUT)
        if arguments.check:
            if not path.exists():
                print(f"{OUTPUT}: no table yet")
                return 1
            current = json.loads(path.read_text(encoding="utf-8"))
            if current != table:
                print(f"{OUTPUT}: out of date")
                return 1
            print(f"{OUTPUT}: up to date ({len(table['functions'])} functions)")
            return 0
        write(root, table)
        print(f"{OUTPUT}: {len(table['functions'])} functions")
    except (CodeHashesError, WorkspaceError, OSError, UnicodeError, KeyError,
            TypeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
