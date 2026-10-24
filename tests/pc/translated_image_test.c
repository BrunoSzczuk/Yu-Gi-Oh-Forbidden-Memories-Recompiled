#include "types.h"
#include "pc/guest/translated_image.h"
#include "pc/guest/code_patches.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); return 1; } } while (0)
/* No retail function to compare an image's code against in this test. */
const MemoriesCodeHash Memories_CodeHashes[] = {{0u, 0u, {0}}};
const unsigned Memories_CodeHashCount = 0u;
const MemoriesCodeRegion Memories_CodeRegions[] = {{0u, 0u}};
const unsigned Memories_CodeRegionCount = 0u;

int main(void)
{
    MemoriesMemory *memory = calloc(1, sizeof(*memory));
    const size_t ai_signature_file = 0x800u + (0x800916E0u - 0x80010000u) + 12u * 4u;
    const size_t valid_image_length = 0x800u + 0x817A4u;
    u8 image[0x82000] = {0};
    u32 entry = 0xfeedfaceu;
    const MemoriesExecutionProfile *profile;
    CHECK(memory != NULL);
    memcpy(image, "PS-X EXE", 8);
    Memories_WriteLE32(image + 0x10, 0x80010000u);
    Memories_WriteLE32(image + 0x18, 0x80010000u);
    Memories_WriteLE32(image + 0x1c, 0x817A4u);
    memset(image + 0x800, 0xa5, 16);
    Memories_WriteLE32(image + ai_signature_file + 0u, 0x80070F1Cu);
    Memories_WriteLE32(image + ai_signature_file + 4u, 0x80070FF8u);
    Memories_WriteLE32(image + ai_signature_file + 8u, 0x80071000u);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == 0);
    CHECK(entry == 0x80010000u);
    profile = Memories_TranslatedExecutionProfile();
    CHECK(profile != NULL);
    CHECK(profile->id == MEMORIES_PROFILE_UNKNOWN);
    CHECK(profile->mode == MEMORIES_EXECUTION_FULL_GUEST);
    CHECK(profile->has_ai_signature);
    CHECK(!profile->has_vanilla_hash);
    CHECK(!profile->layout_compatible);
    CHECK(profile->ai_play_field_card == 0x80070F1Cu);
    CHECK(profile->ai_end_hand == 0x80070FF8u);
    CHECK(profile->ai_end_field == 0x80071000u);
    CHECK(Memories_TranslatedExeContains(0x80010000u));
    CHECK(Memories_TranslatedExeContains(0x800917A3u));
    CHECK(!Memories_TranslatedExeContains(0x800917A4u));
    CHECK(!Memories_TranslatedExeContains(0x801a9400u));
    CHECK(!memcmp(memory->ram + 0x10000, image + 0x800, 16));
    CHECK(memory->ram[0xffff] == 0 && memory->ram[0x10010] == 0);
    Memories_WriteLE32(image + ai_signature_file, 0x8003E80Cu);
    entry = 0xfeedfaceu;
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == 0);
    profile = Memories_TranslatedExecutionProfile();
    CHECK(profile != NULL);
    CHECK(profile->id == MEMORIES_PROFILE_UNKNOWN);
    CHECK(profile->mode == MEMORIES_EXECUTION_FULL_GUEST);
    CHECK(profile->has_ai_signature);
    CHECK(profile->ai_play_field_card == 0x8003E80Cu);
    Memories_WriteLE32(image + 0x1c, 16);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == 0);
    profile = Memories_TranslatedExecutionProfile();
    CHECK(profile != NULL);
    CHECK(profile->id == MEMORIES_PROFILE_UNKNOWN);
    CHECK(profile->mode == MEMORIES_EXECUTION_FULL_GUEST);
    CHECK(!profile->has_ai_signature);
    Memories_WriteLE32(image + 0x1c, 0x817A4u);
    memset(memory->ram + 0x10000, 0x5a, 16);
    entry = 0xfeedfaceu;
    CHECK(Memories_LoadTranslatedExe(memory, image, valid_image_length - 1, &entry) == -1);
    image[0] = 0;
    /* A community "free version" image ships its executable with the magic's
     * first byte wiped and the rest of the header unchanged: it loads. */
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == 0);
    image[0] = 'P';
    image[1] = 0;
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    image[1] = 'S';
    /* The loads that failed left the memory and the entry as they were. */
    memset(memory->ram + 0x10000, 0x5a, 16);
    entry = 0xfeedfaceu;
    Memories_WriteLE32(image + 0x1c, 0);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    Memories_WriteLE32(image + 0x1c, 1);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    Memories_WriteLE32(image + 0x1c, 0xffffffffu);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    Memories_WriteLE32(image + 0x1c, 16);
    Memories_WriteLE32(image + 0x10, 0x80010010u);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    Memories_WriteLE32(image + 0x10, 0x80010001u);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    Memories_WriteLE32(image + 0x18, 0x801ffffcu);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    Memories_WriteLE32(image + 0x18, 0x80000000u);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), &entry) == -1);
    CHECK(Memories_LoadTranslatedExe(NULL, image, sizeof(image), &entry) == -1);
    CHECK(Memories_LoadTranslatedExe(memory, NULL, sizeof(image), &entry) == -1);
    CHECK(Memories_LoadTranslatedExe(memory, image, sizeof(image), NULL) == -1);
    CHECK(Memories_LoadTranslatedExe(memory, image, 7, &entry) == -1);
    CHECK(entry == 0xfeedfaceu && memory->ram[0x10000] == 0x5a);

    /* A community image that keeps the retail layout: the native code still
     * matches the retail functions, and the code the image patched runs in
     * the interpreter. The image is not the retail executable, so it is not
     * the vanilla profile, but it does not need the full guest either. */
    {
        const size_t retail_size = 0x1D0000u;
        const size_t table_at = 0x800u + (0x800916E0u - 0x80010000u);
        unsigned char *retail = calloc(1, 0x800u + 0x1D0800u);
        unsigned i;
        CHECK(retail != NULL);
        memcpy(retail, "PS-X EXE", 8);
        Memories_WriteLE32(retail + 0x10, 0x800129D8u);   /* entry */
        Memories_WriteLE32(retail + 0x18, 0x80010000u);   /* load address */
        Memories_WriteLE32(retail + 0x1c, retail_size);   /* resident size */
        /* Every opcode's handler is an address in the resident image: the
         * retail ones, and the ones a community image points at its own
         * code or its own scripts with. */
        for (i = 1; i < 68u; ++i)
            Memories_WriteLE32(retail + table_at + i * 4u, 0x80071000u + i * 4u);
        CHECK(Memories_LoadTranslatedExe(memory, retail, 0x800u + retail_size, &entry) == 0);
        profile = Memories_TranslatedExecutionProfile();
        CHECK(profile != NULL);
        CHECK(profile->id == MEMORIES_PROFILE_MODIFIED);
        CHECK(profile->mode == MEMORIES_EXECUTION_NATIVE_HYBRID);
        CHECK(profile->layout_compatible);
        CHECK(profile->has_ai_signature);
        CHECK(!profile->has_vanilla_hash);
        /* The same image with a handler outside the resident image is not a
         * community image the native code can stand in for. */
        Memories_WriteLE32(retail + table_at + 42u * 4u, 0x801F0000u);
        CHECK(Memories_LoadTranslatedExe(memory, retail, 0x800u + retail_size, &entry) == 0);
        profile = Memories_TranslatedExecutionProfile();
        CHECK(profile != NULL);
        CHECK(profile->mode == MEMORIES_EXECUTION_FULL_GUEST);
        CHECK(!profile->layout_compatible);
        /* And one with another layout at all. */
        Memories_WriteLE32(retail + table_at + 42u * 4u, 0x80071000u + 42u * 4u);
        Memories_WriteLE32(retail + 0x1c, 0x1D0800u);
        CHECK(Memories_LoadTranslatedExe(memory, retail, 0x800u + 0x1D0800u, &entry) == 0);
        profile = Memories_TranslatedExecutionProfile();
        CHECK(profile != NULL);
        CHECK(profile->mode == MEMORIES_EXECUTION_FULL_GUEST);
        CHECK(!profile->layout_compatible);
        free(retail);
    }
    free(memory);
    puts("Translated PS-X EXE loader: bounds, entry and failure atomicity passed");
    return 0;
}
