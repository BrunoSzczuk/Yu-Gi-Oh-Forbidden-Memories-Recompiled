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
Native arm64: the image changed 4 of the retail functions; they run in the interpreter
Native arm64: the image's own code: 0x80071b64, 0x800727c0, 0x80072a48, 0x8007308c
```

## How the port runs the image's own code

A changed function runs in the MIPS interpreter (`src/pc/guest/mips.c`), not
in the native code. There are two ways into it, and the image's code has the
priority in both:

- **Through the game's own data.** The AI's opcode table is read from the
  image (the table is pinned at its retail address, `0x800916E0`), so a
  handler the patch points at is the patch's code. The interpreter runs it
  and calls the retail functions back as it goes. This is the path the
  Kuriboh image's opcodes take.
- **Through a native call.** The native code's own call to a retail address
  runs the image's code there instead of the retail code. The build gives
  every function with a retail address a wrapper
  (`tools/pc/llvm/guest_ir.cpp`): the wrapper asks whether the image changed
  that address, and if it did, calls the interpreter with the arguments in
  the MIPS registers and takes the answer from it. When the interpreter
  cannot run the image's code, the wrapper falls back to the retail code.

A function whose answer is a host pointer keeps the native code: the
image's code would answer with a guest address, which the native code cannot
use.

## What works today

Checked against the Kuriboh image, on the macOS ARM64 build, through the
control channel (`tools/pc/yfm_control.py`):

- The image loads, the profile is `modified`, and the game boots.
- The title's menu is the mod's own (its `SU.MRG`): the cursor and the
  game's selection agree, and the mod's title screen and art are the ones
  drawn.
- A duel plays: the hand is dealt, a card is summoned, the opponent's turn
  runs, attacks land and the duel ends with the results screen.
- **The opponent plays the mod's rules.** The mod's opcodes run its own
  code, in the interpreter, with the retail handlers called back. Against
  the eight duellists the image ships, the interpreter ran the patch's
  `0x801A9400`, `0x801AA800`, `0x80072A54`, `0x800729D0` and `0x80071B64`:

  ```
  $ MEMORIES_TRACE_GUEST=1 ./tmp/pc/macos/memories-arm64
  memories-pc: guest call 0x801a9400: interpreted
  memories-pc: guest call 0x801aa800: interpreted
  memories-pc: guest call 0x80072a54: interpreted
  ```

- **The drops are dealt.** A duel won puts a card in the chest and pays the
  starchips (`chest after: {280: 1}`, `starchips: 5`). The weights are the
  image's own: the port reads them from `WA_MRG.MRG` (sector 7478), which the
  mod patched.
- The port's own mods work with the image: `3d-monsters` reads the empty
  `MODEL.MRG` and falls back to the card art, and `ai-hard-mode`,
  `hand-camera` and `yamyi-mods` load. The build leaves them beside the game
  (`tmp/pc/macos/mods/`), each with its code built for the machine; before
  that, `python3 tools/pc/build_mod.py mods/<id> --target macos` builds one,
  or the game says the dylib is missing.

## What still does not work

- **A changed function that keeps the retail function's shape is not
  found.** The shape rule reads the prologue and the ending of a function.
  A patch that changes only the instructions in between leaves both alone,
  and only the hash rule would tell. That rule needs the retail executable,
  which is not in this repository.
- **The interpreter is a subset of MIPS I.** It has the integer
  instructions and COP2 (the software GTE); it has no floating point, and a
  few instructions are not in it. A native call whose image's code the
  interpreter cannot run falls back to the retail code, so the patch is
  lost for that call; a call through the game's data (the opcode table)
  cannot fall back to anything else, and the game stops with the address it
  stopped at.
- **An image that does not keep the retail layout is refused.** The port
  classifies the executable it loaded (`src/pc/guest/execution_profile.c`):

  | Profile | What it is | What the port does |
  |---|---|---|
  | vanilla | the retail executable, by SHA-256 | the native path |
  | modified | the retail layout, with a different executable | the native path, and the patch's own code in the interpreter |
  | unknown | neither | refused, with the layout it found in the log |

- The cards the mod adds past the retail 722 are the port's own mod system
  rather than the image: `notes/more-cards.md` and `notes/modding.md` cover
  how a card mod is written. (The Kuriboh image keeps the retail 722 and
  changes what they do.)
- The image ships no 3D models (`MODEL.MRG` is an empty archive), so the
  monsters are their card art. The port's `3d-monsters` mod has no models to
  stand on either.

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

The port's own mods are code mods and have to be built for the machine
before the game can load them:

```sh
for mod in mods/*/; do python3 tools/pc/build_mod.py "$mod" --target macos; done
```

The log says which profile the image was:

```
Execution profile: modified, mode: native-hybrid, entry: 0x800129d8
Native arm64: a modified image with the retail layout; its own code runs in the interpreter
```

`MEMORIES_TRACE_GUEST=1` logs every call that leaves the native code for
the interpreter, and what the interpreter answered, which is how a community
image's own code shows itself.
