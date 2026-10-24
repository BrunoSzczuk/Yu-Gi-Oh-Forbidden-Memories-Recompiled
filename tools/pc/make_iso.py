#!/usr/bin/env python3
"""Turn a raw MODE2/2352 disc image into an ISO 9660 one (2048 bytes a sector).

    python3 tools/pc/make_iso.py "Yu-Gi-Oh! Forbidden Memories (USA).bin" \
        -o "Yu-Gi-Oh! Forbidden Memories (USA).iso"

The port takes either kind of image (notes/disc-layout.md): a raw .bin from a
.bin/.cue pair, or an .iso. A .iso is the same disc without the 24 bytes of
MODE2 header each sector carries, so the conversion drops those bytes and
keeps the sectors where they were: the ISO 9660 volume descriptor stays at
the sector 16 and every file's extent is unchanged.

A sector of a Form 2 (2324 bytes of data) has no place in a 2048-byte
sector, so an image with one cannot be converted as it is: this says so
instead of writing a broken file. (PlayStation discs that hold video use
Form 1 sectors for it, which convert as they are.)
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys

RAW_SECTOR = 2352
ISO_SECTOR = 2048
HEADER = 24  # the MODE2 header a raw image puts before the data
SYSTEM_SECTORS = 16  # the sectors before the ISO 9660 volume descriptor


def read_sector(raw: bytes) -> tuple[bool, bytes]:
    """Whether the sector is a Form 2 one, and its data, from a raw MODE2
    sector. A Form 1 sector holds 2048 bytes of data and the EDC and ECC
    that follow them; a Form 2 one holds 2324 bytes of data instead."""
    if len(raw) < RAW_SECTOR:
        raise ValueError("a sector is short")
    mode = raw[15]
    if mode != 2:
        raise ValueError("the sector is not a MODE2 sector (mode %d)" % mode)
    # The sub-header's submode byte (the 18th of the sector): a Form 2 sector
    # says it holds digital data (0x20) there, a Form 1 one does not.
    form2 = (raw[18] & 0x20) != 0
    size = 2324 if form2 else ISO_SECTOR
    return form2, raw[HEADER:HEADER + size]


def convert(source: Path, target: Path, force: bool = False) -> int:
    """Write the ISO; the sectors it has. Nothing is written on an error."""
    if target.exists() and not force:
        raise SystemExit("%s already exists (use --force to replace it)" % target)
    size = source.stat().st_size
    if size % RAW_SECTOR:
        raise SystemExit("%s is not a whole number of raw sectors (%d bytes)" % (source, size))
    sectors = size // RAW_SECTOR
    temporary = target.with_name(target.name + ".tmp")
    written = 0
    try:
        with source.open("rb") as reader, temporary.open("wb") as writer:
            while True:
                raw = reader.read(RAW_SECTOR)
                if not raw:
                    break
                if len(raw) < RAW_SECTOR:
                    raise SystemExit("%s ends in a partial sector" % source)
                mode, data = read_sector(raw)
                if mode:
                    raise SystemExit(
                        "the sector %d is a Form 2 sector (2324 bytes of data): an ISO 9660 image "
                        "holds 2048 bytes a sector, so this disc cannot be converted as it is"
                        % written)
                writer.write(data)
                written += 1
        if written < SYSTEM_SECTORS + 1:
            raise SystemExit("%s has only %d sectors: too few to be a disc" % (source, written))
        # The volume descriptor must be at the sector 16 for the image to be
        # read as one, and its root directory must lie inside the image.
        with temporary.open("rb") as check:
            check.seek(SYSTEM_SECTORS * ISO_SECTOR)
            descriptor = check.read(ISO_SECTOR)
            if descriptor[:6] != b"\x01CD001":
                raise SystemExit("%s has no ISO 9660 volume descriptor at the sector %d"
                                 % (source, SYSTEM_SECTORS))
            extent = int.from_bytes(descriptor[156 + 2:156 + 6], "little")
            size = int.from_bytes(descriptor[156 + 10:156 + 14], "little")
            if not extent or not size or (extent + (size + ISO_SECTOR - 1) // ISO_SECTOR) > written:
                raise SystemExit("%s has no root directory inside the image" % source)
        os.replace(temporary, target)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise
    return written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path, help="the raw MODE2 .bin image")
    parser.add_argument("-o", "--output", type=Path, help="the .iso to write (default: the source with .iso)")
    parser.add_argument("--force", action="store_true", help="replace the output when it exists")
    options = parser.parse_args(argv)
    target = options.output or options.source.with_suffix(".iso")
    if not options.source.is_file():
        print("no image at %s" % options.source, file=sys.stderr)
        return 1
    sectors = convert(options.source, target, options.force)
    print("%s: %d sectors -> %s" % (options.source.name, sectors, target))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
