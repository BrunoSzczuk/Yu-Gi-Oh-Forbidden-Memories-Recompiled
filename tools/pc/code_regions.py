"""The retail functions' places in the executable, as the C table the runtime
reads (src/pc/guest/code_patches.c).

The table comes from config/slus_01411/functions.csv: each function's
address and size say where the retail code is. The runtime needs no retail
bytes to use it: the retail function's own shape (where it ends, and the
frame its ending restores) is enough to tell an image's code there from the
retail code.

The SDK's own assembly is left out: it is the library's code, not the game's,
and its conventions (a jump stub, a frame the ending does not restore) are
not the game's. The retail hashes, when the retail executable is at hand,
cover those functions exactly.
"""
from typing import Iterable, Mapping


def rows_of(functions: Iterable[Mapping[str, str]]) -> list[tuple[int, int]]:
    """The retail functions that are in the executable: (address, size)."""
    entries: list[tuple[int, int]] = []
    for row in functions:
        if row.get("bank"):
            continue  # an overlay's function is not in the executable
        if row.get("status") == "sdk_asm":
            continue  # the library's own assembly, not the game's code
        try:
            address = int(str(row["address"]), 16)
            size = int(str(row["size"]), 16)
        except (KeyError, TypeError, ValueError):
            continue
        if not size or size < 16:
            continue
        entries.append((address, size))
    entries.sort()
    return entries


def emit(functions: Iterable[Mapping[str, str]]) -> str:
    """The table as C: where each retail function is, and how long it is."""
    entries = rows_of(functions)
    rows = [f"    {{0x{address:08x}u, 0x{size:x}u}},\n" for address, size in entries]
    if not rows:
        # An empty array is not C: one unused entry, and a count of none.
        rows.append("    {0u, 0u},\n")
    return ("const MemoriesCodeRegion Memories_CodeRegions[] = {\n" + "".join(rows) +
            "};\n" + f"const unsigned Memories_CodeRegionCount = {len(entries)}u;\n")
