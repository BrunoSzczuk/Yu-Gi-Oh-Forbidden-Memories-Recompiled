"""The retail code's hashes, as the C table the runtime reads
(src/pc/guest/code_patches.c). The table comes from
config/slus_01411/code_hashes.json, which tools/project/code_hashes.py
writes from the retail executable; without it the runtime has no retail
function to compare an image's code against, and every image runs the
native code."""
import json
from pathlib import Path

CONFIG = "config/slus_01411/code_hashes.json"


def functions(root) -> list[dict]:
    path = Path(root) / CONFIG
    if not path.exists():
        return []
    table = json.loads(path.read_text(encoding="utf-8"))
    if table.get("schema") != 1:
        raise ValueError(f"{CONFIG}: unsupported schema")
    entries = table.get("functions")
    if not isinstance(entries, list):
        raise ValueError(f"{CONFIG}: no functions list")
    return entries


def emit(root) -> str:
    """The table as C: the hashes, and how many there are."""
    entries = functions(root)
    rows = []
    for entry in entries:
        digest = str(entry["sha256"])
        if len(digest) != 64:
            raise ValueError(f"{CONFIG}: a hash is not 64 characters")
        octets = ",".join(f"0x{digest[at:at + 2]}" for at in range(0, 64, 2))
        rows.append(f"    {{0x{int(str(entry['address']), 16):08x}u, "
                    f"0x{int(str(entry['size']), 16):x}u, {{{octets}}}}},\n")
    if not rows:
        # An empty array is not C: one unused entry, and a count of none.
        rows.append("    {0u, 0u, {0}},\n")
    return ("const MemoriesCodeHash Memories_CodeHashes[] = {\n" + "".join(rows) +
            "};\n" + f"const unsigned Memories_CodeHashCount = {len(entries)}u;\n")
