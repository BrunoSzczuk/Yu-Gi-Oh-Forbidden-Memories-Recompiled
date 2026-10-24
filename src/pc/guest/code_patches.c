/* Which of the retail functions a community image changed.
 *
 * The port's native code is the retail code, recompiled. A community image
 * that patches a function at its retail address would otherwise run the
 * retail code there, and the patch would be silently lost. A function the
 * image changed runs in the interpreter instead: the interpreter runs the
 * image's own code, and the native code calls back into it the way it calls
 * the retail functions.
 *
 * Two rules tell a changed function from an unchanged one, and they need
 * different references:
 *
 * - The hash rule compares the image's bytes at a retail function's address
 *   with the retail bytes (the build's table). It is exact, and it needs the
 *   retail executable, which the repository does not carry: a build without
 *   that table simply has no hash to compare against.
 * - The shape rule needs no retail bytes at all. The retail function's place
 *   and size are known, so the code the image put there can be told from the
 *   retail function's own shape: the retail function ends with `jr $ra` and
 *   restores the frame its prologue set up, and the image's code at that
 *   place either ends elsewhere or does not restore that frame. Either way
 *   it is not the retail function any more.
 */
#include "code_patches.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The functions the image changed: their addresses, in the order they were
 * found. The table is small (the retail image's functions), so a linear
 * walk is enough. */
static uint32_t patched[256];
static unsigned patched_count;
static int patched_valid;

/* The MIPS words the shape rule reads. */
#define MIPS_JR_RA 0x03E00008u    /* jr $ra */
#define MIPS_ADDIU_SP 0x27BD0000u /* addiu $sp,$sp,imm */

static uint32_t load32(const uint8_t *code)
{
    return (uint32_t)code[0] | ((uint32_t)code[1] << 8) |
           ((uint32_t)code[2] << 16) | ((uint32_t)code[3] << 24);
}

/* The stack frame a prologue sets up: `addiu $sp,$sp,-N` gives a frame of N
 * bytes. The ending of the same function restores it with `addiu $sp,$sp,N`. */
static int frame_size(uint32_t word, int *size)
{
    int immediate;
    if ((word & 0xFFFF0000u) != MIPS_ADDIU_SP) return 0;
    immediate = (int)(int16_t)(uint16_t)(word & 0xFFFFu);
    if (immediate >= 0) return 0;
    *size = -immediate;
    return 1;
}

static int frame_restored(uint32_t word, int *size)
{
    int immediate;
    if ((word & 0xFFFF0000u) != MIPS_ADDIU_SP) return 0;
    immediate = (int)(int16_t)(uint16_t)(word & 0xFFFFu);
    if (immediate <= 0) return 0;
    *size = immediate;
    return 1;
}

/* Whether the code at a retail function's place is the image's own and not
 * the retail function. The code is the image's bytes at that place, and
 * `size` is the retail function's size.
 *
 * A function that ends where the retail one ends must restore the frame the
 * retail one set up: the image's prologue and the image's ending have to
 * agree. A function that does not end there is not the retail function: it
 * ends earlier (the image wrote a shorter function) or it never returns
 * there (the retail function did not end that way either, and the image's
 * code cannot be told from the retail one without the retail bytes).
 */
static int shape_changed(const uint8_t *code, uint32_t size)
{
    uint32_t offset;
    int prologue_frame;
    int ending_frame;

    if (size < 16u) return 0;
    if (load32(code + size - 8u) != MIPS_JR_RA) {
        /* The retail function's own ending is not here. The image's code
         * ends earlier than the retail function did, so it is not it. */
        for (offset = 0; offset + 4u <= size - 8u; offset += 4u)
            if (load32(code + offset) == MIPS_JR_RA) return 1;
        return 0;
    }
    /* The retail ending is here: the frames have to be the same. A function
     * that sets up no frame (a leaf routine) says nothing either way. */
    if (!frame_size(load32(code), &prologue_frame)) return 0;
    if (!frame_restored(load32(code + size - 4u), &ending_frame)) return 0;
    return ending_frame != prologue_frame;
}

void Memories_CodePatchesReset(void)
{
    patched_count = 0;
    patched_valid = 0;
}

void Memories_CodePatchesScan(const uint8_t *image, size_t length, uint32_t load_address)
{
    unsigned i;
    patched_count = 0;
    patched_valid = 0;
    if (!image) return;
    /* The exact rule: the image's bytes against the retail ones. */
    for (i = 0; i < Memories_CodeHashCount; ++i) {
        const MemoriesCodeHash *function = &Memories_CodeHashes[i];
        uint8_t digest[32];
        size_t offset;
        if (!function->size) continue;
        if (function->address < load_address) continue;
        offset = (size_t)(function->address - load_address);
        if (offset > length || length - offset < function->size) continue;
        Sha256(image + offset, function->size, digest);
        if (!memcmp(digest, function->sha256, sizeof(digest))) continue;
        if (patched_count < sizeof(patched) / sizeof(patched[0]))
            patched[patched_count++] = function->address;
    }
    /* The shape rule: the image's code against the retail function's own
     * shape, which needs no retail bytes. */
    for (i = 0; i < Memories_CodeRegionCount; ++i) {
        const MemoriesCodeRegion *region = &Memories_CodeRegions[i];
        size_t offset;
        if (!region->size) continue;
        if (region->address < load_address) continue;
        offset = (size_t)(region->address - load_address);
        if (offset > length || length - offset < region->size) continue;
        if (!shape_changed(image + offset, region->size)) continue;
        if (patched_count >= sizeof(patched) / sizeof(patched[0])) break;
        patched[patched_count++] = region->address;
    }
    patched_valid = 1;
}

int Memories_CodePatched(uint32_t address)
{
    unsigned i;
    if (!patched_valid) return 0;
    for (i = 0; i < patched_count; ++i)
        if (patched[i] == address) return 1;
    return 0;
}

/* MEMORIES_TRACE_GUEST=1 logs the calls that leave the native code for the
 * interpreter: the code a community image patched runs this way. */
static int trace_guest(void)
{
    static int tracing = -1;
    const char *value;
    if (tracing >= 0) return tracing;
    value = getenv("MEMORIES_TRACE_GUEST");
    tracing = value && *value && strcmp(value, "0") != 0;
    return tracing;
}

/* Whether the native code's own call to this address has to run the image's
 * code instead of the retail code. */
int Memories_CodePatchedCall(uint32_t address)
{
    if (!Memories_CodePatched(address)) return 0;
    if (trace_guest())
        fprintf(stderr, "memories-pc: native call 0x%08x: the image's own code "
                "(interpreted)\n", address);
    return 1;
}

unsigned Memories_CodePatchedCount(void)
{
    return patched_count;
}

const uint32_t *Memories_CodePatchedAddresses(void)
{
    return patched;
}
