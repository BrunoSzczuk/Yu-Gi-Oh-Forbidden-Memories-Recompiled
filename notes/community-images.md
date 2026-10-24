# Community images

A community image is a disc image of Forbidden Memories that someone patched
to change the game: new cards, new rules, new drops. The port runs a patched
image when the patch kept the retail layout, and it reads the patch's data
from the image itself.

This note records what a patched image looks like from the port's side, what
the port does with it, and what it still cannot do. The example is the
`Yu-Gi-Oh! Kuriboh Evolution (Drop x15)` image, the one the port was tested
against.

## What the image is

The image is the retail disc with three files patched in place, at the same
sizes, and one file emptied:

| File | Retail size | Patched size | Change |
|---|---|---|---|
| `SLUS_014.11` | 1902592 | 1902592 | the executable, patched in place |
| `DATA/SU.MRG` | 2537472 | 2537472 | the screen package, patched in place |
| `DATA/WA_MRG.MRG` | 37748736 | 37748736 | the asset package, patched in place |
| `DATA/MODEL.MRG` | 351019008 | 24 | emptied: the image ships no 3D models |
| the rest | | | byte for byte the retail files |

The file sizes are the retail ones, so the patch was made by a tool that
rewrites files in place. The disc image itself is a MODE2 image with an
8-byte subheader per sector (`tools/project/disc_image.py` reads it).

The empty `MODEL.MRG` is a RAR archive with no members. The port's 3D
monsters need the models, so an image like this one has no 3D monsters: the
port falls back to the 2D cards for a monster whose model is missing.

## What the patch changed in the executable

The executable keeps the retail layout: it loads at `0x80010000`, it is
`0x1D0000` bytes of resident image, and it enters at `0x800129D8`. The
region map (`config/slus_01411/image_map.json`) still describes its parts.
Nothing moved: every function of the retail executable is still at its
retail address, and the patch wrote over the code that was there.

The patch is in the AI's opcode table at `0x800916E0`. Sixty-eight entries
point at the handlers of `AiScript_Run`'s dispatch; eight of them differ
from the retail ones:

| Opcode | Retail handler | Patched handler |
|---|---|---|
| 32 | `0x80071B64` (`AiScript_FindKiller`) | `0x800727C0` (`Ai_CompleteFusion`) |
| 42 | `0x80072A48` (`AiScript_FindBestCombo`) | `0x801A9400` (the patch's own script) |
| 43 | `0x80072DC0` (`AiScript_EvaluateFusion`) | `0x80071B64` (`AiScript_FindKiller`) |
| 48 | `0x8007308C` (`AiScript_FindBestAttack`) | `0x801AA800` (the patch's own script) |
| 59 | `0x80073464` | `0x8007308C` (`AiScript_FindBestAttack`) |
| 60 | `0x80073474` | `0x80073164` (inside the patch's `AiScript_FindBestAttack`) |
| 66 | `0x8007368C` (`AiScript_LoadGuardianStarChoice`) | `0x80072A54` (inside the patch's `AiScript_FindBestCombo`) |
| 67 | `0x800736C4` (`AiScript_Print`) | `0x800729D0` (inside the patch's `Ai_CompleteFusion`) |

Three of the patched entries point *inside* a retail function. A patch that
gives a retail function more than one entry point is a MIPS patch: the
handler the port has for `0x80072A48` is the retail one, and it is not what
runs at `0x80072A54`.

Two entries point into the screen package's bank (`0x80180000` and up), which
the patch fills with its own AI scripts. `DATA/SU.MRG` is where they come
from: the executable itself holds zeroes there.

The patch did more than the table. Four retail functions were replaced where
they stand, with code of the patch's own:

| Address | Retail function | Size |
|---|---|---|
| `0x80071B64` | `AiScript_FindKiller` | `0x14C` |
| `0x800727C0` | `Ai_CompleteFusion` | `0x288` |
| `0x80072A48` | `AiScript_FindBestCombo` | `0x378` |
| `0x8007308C` | `AiScript_FindBestAttack` | `0x194` |

The three entry points the table adds (`0x800729D0`, `0x80072A54`,
`0x80073164`) are inside those four replacements: the patch gave its own
code more than one way in. What the patch changed, then, is how the opponent
plays: the AI's opcodes, the way it finds a killer, the way it evaluates a
fusion, and the way it picks an attack.

## How the port tells a changed function from a retail one

The port's native code is the retail code, recompiled from the retail
executable (`src/game/`). A function the image replaced would therefore run
the retail code and lose the patch. Two rules tell the port that the image
changed a function (`src/pc/guest/code_patches.c`):

- The **hash rule** compares the image's bytes at a function's address with
  the retail bytes, function by function. It is exact. The retail bytes come
  from the retail disc, which this repository does not carry, so the table
  (`config/slus_01411/code_hashes.json`) is absent and the rule is inert
  until someone with the disc writes it:
  `python3 tools/project/code_hashes.py`.
- The **shape rule** needs no retail bytes. A retail function ends with
  `jr $ra` and restores the frame its prologue set up; code that ends
  elsewhere, or that restores a different frame, is not the retail function.
  The retail functions' places and sizes are in
  `config/slus_01411/functions.csv`, which the build turns into
  `Memories_CodeRegions[]` (`tools/pc/code_regions.py`).

On the Kuriboh image the shape rule finds exactly the four functions the
patch replaced, and nothing else:

```
Native arm64: the image changed 4 of the retail functions: 0x80071b64, 0x800727c0, 0x80072a48, 0x8007308c
```

## What works today

Checked against the Kuriboh image, on the macOS ARM64 build:

- The image loads, its profile is `modified`, and the game boots.
- A retail disc keeps its own profile (`vanilla`) and runs the native path,
  as before.
- The functions the image changed are found, and the port says which ones they
  are when the image is loaded.

An image whose layout is not the retail one is refused, with the layout it
found, instead of running the retail code against an executable that is not
the retail one:

```
Execution profile: unknown, mode: full-guest, entry: 0x800129d8
Native arm64: this executable does not have the retail layout (load 0x80010000, size 0x001d0000, entry 0x800129d8); the port has no native counterpart for it and cannot run it
```

## Running an image

The port takes the image the same way it takes the retail disc: point it at
the `.bin` (Android's `Import mod...` and the desktop's first run both take
an image). Nothing has to be converted first.

```sh
MEMORIES_DISC="$HOME/Downloads/Yu-Gi-Oh! Kuriboh Evolution (Drop x15).bin" \
    ./tmp/pc/macos/memories-arm64
```

An `.iso` of the same disc is taken as well, and read the same way
([disc layout](disc-layout.md#the-image-the-port-reads)):

```sh
MEMORIES_DISC="$HOME/Downloads/Yu-Gi-Oh! Kuriboh Evolution (Drop x15).iso" \
    ./tmp/pc/macos/memories-arm64
```

A `.bin` becomes an `.iso` with:

```sh
python3 tools/pc/make_iso.py "$HOME/Downloads/Yu-Gi-Oh! Kuriboh Evolution (Drop x15).bin"
```

The first run asks for the image and remembers it
(`disc-path.txt` in the user directory), so the next run needs no variable.
