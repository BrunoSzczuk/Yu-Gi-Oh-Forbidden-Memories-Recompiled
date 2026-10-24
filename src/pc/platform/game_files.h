#ifndef MEMORIES_PC_GAME_FILES_H
#define MEMORIES_PC_GAME_FILES_H
/* The player's own copy of the game: an image of the USA disc, SLUS-01411,
 * either raw (MODE2/2352, the .bin of a .bin/.cue pair) or ISO 9660 (.iso).
 * Nothing of the game ships with the port, so this is the one file a player
 * has to bring.
 *
 * MEMORIES_DISC names it outright. Otherwise the remembered selection, then the first image of the right
 * disc is taken from, in turn: the `game` folder beside the executable, the
 * folder of the executable itself, the `game` folder in the user directory
 * (paths.h), and `game` in the current directory. Any file name will do; a
 * .bin or .iso that is not that disc is passed over. */
#include <stddef.h>
#include <stdio.h>

/* The image's path, or NULL with the reason (for the player) in `why`. */
const char *GameFiles_Disc(char *why, size_t why_size);
/* Interactive first-run setup when discovery fails. 1: ready, 0: cancelled,
 * -1: failed. Headless and explicit MEMORIES_DISC failures never prompt. */
int GameFiles_Setup(char *why, size_t why_size);
/* Validate and remember a selected image. 0 on success, -1 with why. */
int GameFiles_SelectDisc(const char *path, char *why, size_t why_size);
/* The game's executable, SLUS_014.11, read out of the disc image into a
 * fresh buffer (free it). NULL when it cannot be read. */
unsigned char *GameFiles_ReadExecutable(const char *disc, size_t *size);

/* Plain reads of a raw (2352) or ISO 9660 (2048) image, for other discs of
 * the game too (the PAL discs of Game > Language, language.h): the 2048
 * bytes of user data of sector `lba`, and where the ISO 9660 file `path`
 * is ("DATA/WA_MRG.MRG", names without ";1"). 1 on success. */
int GameFiles_ReadSector(FILE *file, unsigned lba, unsigned char *out);
int GameFiles_FindFile(FILE *file, const char *path, unsigned *lba, unsigned *size);
/* The image's sector size: 2352 for a raw MODE2 image (the .bin of a
 * .bin/.cue pair), 2048 for an ISO 9660 image (.iso), 0 when the file is
 * not a disc image. The volume descriptor at the sector 16 tells which. */
int GameFiles_SectorSize(int fd);

#endif
