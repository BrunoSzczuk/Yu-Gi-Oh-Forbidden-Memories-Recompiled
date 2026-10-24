/* Which of the retail functions a community image changed.
 *
 * The rules need two references: the retail bytes (the build's hash table)
 * and the retail function's own shape (the build's table of places). This
 * test has tables of its own, so that both rules can be checked without the
 * retail disc.
 */
#include "pc/guest/code_patches.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FIRST 0x80070000u
#define SECOND 0x80070100u
#define THIRD 0x80070200u
#define FOURTH 0x80070300u
#define SIZE 16

/* The MIPS words the rules read. */
#define JR_RA 0x03E00008u
#define ADDIU_SP 0x27BD0000u

static uint8_t retail[0x400];

/* The retail functions, as the build's table would have them: the hashes of
 * the bytes this test puts in the image. */
const MemoriesCodeHash Memories_CodeHashes[] = {
    {FIRST, SIZE,
     {0xfc, 0x2e, 0x2c, 0x73, 0x07, 0x2b, 0xfa, 0x2b, 0xda, 0x03, 0xff, 0x93,
      0x07, 0x47, 0x2d, 0xeb, 0xd3, 0xcc, 0x81, 0x05, 0x02, 0x8a, 0x8a, 0x9e,
      0x23, 0x5e, 0x35, 0xba, 0x8d, 0x2e, 0x37, 0xf4}},
    {SECOND, SIZE,
     {0xba, 0x22, 0xb7, 0xdc, 0x95, 0xf6, 0xcc, 0x87, 0x65, 0x75, 0x7b, 0xe4,
      0xbc, 0xcf, 0x37, 0xcd, 0x92, 0xec, 0xe6, 0xd4, 0x98, 0x7d, 0xc2, 0x6a,
      0x31, 0xe2, 0x74, 0xc9, 0xbe, 0x23, 0x69, 0x21}},
};
const unsigned Memories_CodeHashCount = 2;

/* The retail functions' places, as the build's table has them: where each
 * one is and how long it is. The last two are not in the hash table: the
 * shape rule has to find them on its own. */
const MemoriesCodeRegion Memories_CodeRegions[] = {
    {FIRST, SIZE},
    {SECOND, SIZE},
    {THIRD, SIZE},
    {FOURTH, SIZE},
};
const unsigned Memories_CodeRegionCount = 4;

static void put(uint32_t address, uint32_t word)
{
    uint8_t *at = retail + (address - FIRST);
    at[0] = (uint8_t)(word & 0xFF);
    at[1] = (uint8_t)((word >> 8) & 0xFF);
    at[2] = (uint8_t)((word >> 16) & 0xFF);
    at[3] = (uint8_t)((word >> 24) & 0xFF);
}

static void fill(uint32_t address, uint8_t seed)
{
    unsigned i;
    for (i = 0; i < SIZE; ++i) retail[address - FIRST + i] = (uint8_t)(seed + i);
}

/* A retail function: a frame, some code, and the ending that restores the
 * frame and returns. */
static void retail_function(uint32_t address, int frame)
{
    unsigned i;
    for (i = 0; i + 4u < SIZE; i += 4u) put(address + i, 0x00000000u);
    put(address, ADDIU_SP | (uint32_t)(0x10000u - (uint32_t)frame));
    put(address + SIZE - 8u, JR_RA);
    put(address + SIZE - 4u, ADDIU_SP | (uint32_t)frame);
}

int main(void)
{
    /* The image as it is: none of the functions changed. */
    fill(FIRST, 0x10);
    fill(SECOND, 0x40);
    retail_function(THIRD, 0x30);
    retail_function(FOURTH, 0x30);
    Memories_CodePatchesScan(retail, sizeof(retail), FIRST);
    assert(Memories_CodePatchedCount() == 0);
    assert(!Memories_CodePatched(FIRST));
    assert(!Memories_CodePatched(SECOND));
    assert(!Memories_CodePatched(THIRD));
    assert(!Memories_CodePatched(FOURTH));

    /* One byte changed in the second function: that one is the image's. */
    retail[SECOND - FIRST + 3] ^= 0xFF;
    Memories_CodePatchesScan(retail, sizeof(retail), FIRST);
    assert(Memories_CodePatchedCount() == 1);
    assert(!Memories_CodePatched(FIRST));
    assert(Memories_CodePatched(SECOND));
    retail[SECOND - FIRST + 3] ^= 0xFF;

    /* The frame the ending restores is not the one the prologue set up:
     * the image wrote over one of them, and the shape says so even without
     * the retail bytes. */
    put(THIRD, ADDIU_SP | (0x10000u - 0x38u));
    Memories_CodePatchesScan(retail, sizeof(retail), FIRST);
    assert(Memories_CodePatchedCount() == 1);
    assert(Memories_CodePatched(THIRD));
    assert(!Memories_CodePatched(FIRST));

    /* The image's function ends before the retail one did: it is a shorter
     * function, and not the retail one. */
    retail_function(THIRD, 0x30);
    retail_function(FOURTH, 0x30);
    put(FOURTH + 4u, JR_RA);
    put(FOURTH + 8u, 0x00000000u);
    put(FOURTH + SIZE - 8u, 0x00000000u);
    put(FOURTH + SIZE - 4u, 0x00000000u);
    Memories_CodePatchesScan(retail, sizeof(retail), FIRST);
    assert(Memories_CodePatchedCount() == 1);
    assert(Memories_CodePatched(FOURTH));
    assert(!Memories_CodePatched(FIRST));

    /* The table is empty: nothing is known, so nothing is called changed. */
    Memories_CodePatchesReset();
    assert(Memories_CodePatchedCount() == 0);
    assert(!Memories_CodePatched(SECOND));
    puts("Code patches: changed functions found by their retail hash and by their shape");
    return 0;
}
