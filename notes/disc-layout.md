# Original Disc Layout

## Scope

The original BIN/CUE is an immutable reference for ISO9660 LBAs, streaming
order, and extracted-file validation. The project still targets a byte-identical
`SLUS_014.11`; rebuilding a full disc image is not currently a completion
requirement.

`make verify-disc` scans the raw MODE2/2352 image, verifies the tracked layout
at `config/slus_01411/disc_layout.json`, and confirms that every extracted
SLUS/DATA file matches its disc extent byte-for-byte.

## Track

| Property | Value |
|---|---|
| CUE | `game/rpg-yfm.cue` |
| BIN | `game/rpg-yfm.bin` |
| CUE reference | `RPG-YFM.BIN` |
| Track | 1 |
| Mode | `MODE2/2352` |
| Raw sectors | 220,184 |
| Raw sector size | 2,352 bytes |
| Logical block size | 2,048 bytes |
| BIN SHA-256 | `6e22494a45bf50fa2d239cd3819a57163a5f9b91e0365babc3e101509b5c3a7c` |

The CUE uses an uppercase filename while the local BIN is lowercase. The disc
tool resolves that reference case-insensitively without renaming either input.

## ISO9660 files

MSF values include the standard 150-sector CD lead-in.

| Path | LBA | MSF | Size |
|---|---:|---:|---:|
| `SYSTEM.CNF` | 23 | `00:02:23` | 68 |
| `SLUS_014.11` | 24 | `00:02:24` | 1,902,592 |
| `DATA/SU.MRG` | 954 | `00:14:54` | 2,537,472 |
| `DATA/SD_SE.DAT` | 2,193 | `00:31:18` | 1,521,664 |
| `DATA/SD_BGM.DAT` | 2,936 | `00:41:11` | 14,675,968 |
| `DATA/WA_MRG.MRG` | 10,102 | `02:16:52` | 37,748,736 |
| `DATA/MODEL.MRG` | 28,534 | `06:22:34` | 351,019,008 |
| `DATA/MASTER.XA` | 199,930 | `44:27:55` | 5,742,592 |
| `DATA/MOVIE.STR` | 202,734 | `45:05:09` | 35,430,400 |

`SYSTEM.CNF` contains:

```text
BOOT = cdrom:\SLUS_014.11;1
TCB = 4
EVENT = 16
STACK = 801FFF00
```

The disc directory order places the executable and small data files first,
followed by the large model archive, then XA audio and movie data at the end.
This order and the exact LBAs must be retained when investigating streaming or
seek behavior.

## The image the port reads

The port takes the disc as an image of the game, and reads either kind of it:

| Kind | Sector size | What the sector holds | How it is told apart |
|---|---:|---|---|
| raw `.bin` | 2,352 bytes | a 24-byte MODE2 header, then the 2,048 bytes of data | the volume descriptor at the sector 16 has `CD001` after the header |
| `.iso` | 2,048 bytes | the 2,048 bytes of data | the volume descriptor at the sector 16 has `CD001` at its front |

Both carry the same ISO 9660 filesystem at the same LBAs; the raw one only
wraps each sector in the MODE2/2352 frame the drive reads. The port tells
them apart by the volume descriptor (`GameFiles_SectorSize`,
`src/pc/platform/game_files.c`) and reads the sectors of either
(`GameFiles_ReadSector`).

The drive hands the guest the MODE2/Form1 framing in both cases
(`src/pc/sdk/libds.c`): an ISO 9660 sector has no header of its own, so the
drive builds the frame around the data it read, and the rest of the port
(directory, file transfers, STR, XA) is the same for either image.

A `.bin` becomes an `.iso` by dropping the 24 bytes of header of each sector;
the sectors keep their places, so the volume descriptor stays at the sector
16 and every file's extent is unchanged:

```sh
python3 tools/pc/make_iso.py "Yu-Gi-Oh! Forbidden Memories (USA).bin" \
    -o "Yu-Gi-Oh! Forbidden Memories (USA).iso"
```

A sector with 2,324 bytes of data (a Form 2 sector) has no place in a
2,048-byte sector, and the tool says so instead of writing a broken image.

The executable is the file the disc's own `SYSTEM.CNF` names
(`BOOT = cdrom:\SLUS_014.11;1` on the retail disc). A community image may
rename it — the "Galaxy of Gods" image calls it `GOG_VNT.223` — and the
port follows the `SYSTEM.CNF` rather than the retail name. The image's
data segment may also be shorter than its own header says (the zeros at
its end trimmed away), so the port reports the sectors it read, not the
bytes the file holds.

