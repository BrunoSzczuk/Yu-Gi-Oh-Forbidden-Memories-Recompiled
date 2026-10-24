/* The community drop mod's count, read out of the disc's executable
 * (notes/card-drops.md, "The community drop mod").
 *
 * The two community discs this was written for (Kuriboh Evolution and
 * Remastered Perfected, both named "Drop x15") patch the results screen the
 * same way: a jump where the retail draws the prize, to a loop of the kit's
 * own that draws a card as many times as a limit allows. The disc's
 * executable holds a copy of that loop with a limit of six (five cards); the
 * screen package the game loads for a duel holds the copy that runs, with a
 * limit of sixteen (the fifteen cards the discs are named for). The kit's
 * other shape keeps the limit in a byte of its own, which the port cannot
 * read (the kit's code of that shape lives in the executable's header, which
 * the port does not load).
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "pc/debug/log.h"

int Log_Wanted(LogChannel channel) { (void)channel; return 0; }
void Log_Printf(LogChannel channel, const char *format, ...) { (void)channel; (void)format; }

/* The mode the guest is in (main_mode_state.h): the module reads it to tell
 * the kit's code that runs at the results screen from the older copy the
 * disc's executable holds. */
unsigned char D_8009B26C;

#include "../../src/pc/cards/community_drops.c"

#define IMAGE_BASE 0x80010000u
#define IMAGE_SIZE 0x1D0000u

static unsigned char *image;

static void put(uint32_t address, uint32_t word)
{
    uint32_t at = address - IMAGE_BASE;
    image[at] = (unsigned char)(word & 0xFF);
    image[at + 1] = (unsigned char)((word >> 8) & 0xFF);
    image[at + 2] = (unsigned char)((word >> 16) & 0xFF);
    image[at + 3] = (unsigned char)((word >> 24) & 0xFF);
}

int main(void)
{
    image = calloc(IMAGE_SIZE, 1);
    assert(image);

    /* The retail's own code at the prize's draw: no patch, one card a win. */
    put(0x80021C6C, 0xAFA40000u);
    assert(CommunityDrops_CountInImage(image, IMAGE_SIZE) == 0);

    /* The community patch: the jump to the kit's loop and the loop's limit. */
    put(0x80021C6C, 0x0806AB10u); /* j 0x801AAC40 */
    put(0x801AAC40, 0x3C1D001Bu); /* lui $sp, 0x1b */
    put(0x801AAC44, 0x27BDAC00u); /* addiu $sp, $sp, -0x5400 */
    put(0x801AAC68, 0xA3A00020u); /* sb $zero, 0x20($sp) */
    put(0x801AAC6C, 0x93B60020u); /* lbu $s6, 0x20($sp) */
    put(0x801AAC70, 0x00000000u); /* nop */
    put(0x801AAC74, 0x26D60001u); /* addiu $s6, $s6, 1 */
    put(0x801AAC78, 0x24170006u); /* addiu $s7, $zero, 6 */
    put(0x801AAC7C, 0x12D7001Du); /* beq $s6, $s7, +0x1d */
    assert(CommunityDrops_CountInImage(image, IMAGE_SIZE) == 5);

    /* A limit of sixteen is the fifteen cards the community's x15 patches
     * deal. */
    put(0x801AAC78, 0x24170010u);
    assert(CommunityDrops_CountInImage(image, IMAGE_SIZE) == 15);

    /* A jump out of the image is not the kit's code. */
    put(0x80021C6C, 0x0A000000u); /* j 0x80000000 */
    assert(CommunityDrops_CountInImage(image, IMAGE_SIZE) == 0);

    /* A loop without a limit says nothing about the count. */
    put(0x80021C6C, 0x0806AB10u);
    put(0x801AAC78, 0x00000000u); /* nop */
    assert(CommunityDrops_CountInImage(image, IMAGE_SIZE) == 0);

    /* A disc whose executable is not there is not a patched one. */
    assert(CommunityDrops_CountInImage(image, 0x1000) == 0);

    free(image);
    return 0;
}
