#include "execution_profile.h"
#include "image_loader.h"
#include "pc/memory_endian.h"
#include "sha256.h"
#include <string.h>

/* The native build targets this exact PS-X EXE. A data-only mod can preserve
 * this executable and remain on the native path; a code mod cannot. */
#define VANILLA_EXE_FILE_SIZE 0x001D0800u
#define VANILLA_EXE_SHA256 \
    "84a54ed74f3d0edd6d81380839f7e4ef5bfb21ecea18be9a062bd6bfa5a45c88"
/* The retail layout (config/slus_01411/image_map.json): the load address,
 * the size of the resident image, the entry point and the AI dispatch table
 * that every community image that keeps the layout still has. */
#define VANILLA_LOAD_ADDRESS 0x80010000u
#define VANILLA_IMAGE_SIZE 0x001D0000u
#define VANILLA_ENTRY 0x800129D8u
#define AI_COMMAND_TABLE 0x800916E0u
#define AI_COMMAND_COUNT 68u
#define AI_PLAY_FIELD_CARD_INDEX 12u
#define AI_END_HAND_INDEX 13u
#define AI_END_FIELD_INDEX 14u

static int image_has_word(const MemoriesExeImage *image, uint32_t address)
{
    return address >= image->address &&
           address - image->address <= image->size - sizeof(uint32_t);
}

static int digest_matches_vanilla(const uint8_t digest[32])
{
    static const char expected[] = VANILLA_EXE_SHA256;
    static const char hex[] = "0123456789abcdef";
    char actual[sizeof(expected)];
    unsigned i;

    for (i = 0; i < 32u; ++i) {
        actual[i * 2] = hex[digest[i] >> 4];
        actual[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    actual[sizeof(actual) - 1] = '\0';
    return !memcmp(actual, expected, sizeof(actual) - 1);
}

/* Whether the image still has the retail layout: the same load address, the
 * same resident size, the same entry point, and the AI dispatch table where
 * the retail one is. A community image that keeps the layout can run on the
 * native path: its data comes from its own files, and the code it patched
 * runs in the interpreter. An image that does not keep the layout has no
 * native counterpart at all. */
static int layout_compatible(const MemoriesExeImage *image)
{
    uint32_t end;
    unsigned i;

    if (image->address != VANILLA_LOAD_ADDRESS) return 0;
    if (image->size != VANILLA_IMAGE_SIZE) return 0;
    if (image->entry != VANILLA_ENTRY) return 0;
    if (!image_has_word(image, AI_COMMAND_TABLE + (AI_COMMAND_COUNT - 1u) * 4u)) return 0;
    end = image->address + image->size;
    /* Opcode 0 is unused and its slot is empty in the retail table too. */
    if (Memories_LoadLE32(image->data + (AI_COMMAND_TABLE - image->address)) != 0) return 0;
    for (i = 1u; i < AI_COMMAND_COUNT; ++i) {
        uint32_t handler = Memories_LoadLE32(image->data + (AI_COMMAND_TABLE - image->address) + i * 4u);
        /* Every handler is an address in the resident image. A community
         * image may point an opcode at code or scripts of its own. */
        if (handler < image->address || handler >= end) return 0;
    }
    return 1;
}

int Memories_ExecutionProfileDetect(const uint8_t *data, size_t length,
                                    MemoriesExecutionProfile *profile)
{
    MemoriesExeImage image;
    uint32_t play_field_card;
    uint32_t end_hand;
    uint32_t end_field;

    if (!profile) return -1;
    *profile = (MemoriesExecutionProfile){0};
    if (Memories_ParseExe(data, length, 1, &image)) return -1;

    profile->load_address = image.address;
    profile->image_size = image.size;
    profile->entry = image.entry;
    profile->mode = MEMORIES_EXECUTION_FULL_GUEST;
    profile->id = MEMORIES_PROFILE_UNKNOWN;
    Sha256(data, length, profile->executable_sha256);
    profile->has_vanilla_hash = length == VANILLA_EXE_FILE_SIZE &&
                                digest_matches_vanilla(profile->executable_sha256);
    if (profile->has_vanilla_hash) {
        profile->id = MEMORIES_PROFILE_VANILLA;
        profile->mode = MEMORIES_EXECUTION_NATIVE_HYBRID;
    } else if (layout_compatible(&image)) {
        /* A community image that keeps the retail layout: the native code
         * still matches the retail functions, and the code the image patched
         * runs in the interpreter. */
        profile->id = MEMORIES_PROFILE_MODIFIED;
        profile->mode = MEMORIES_EXECUTION_NATIVE_HYBRID;
        profile->layout_compatible = 1;
    }

    /* These three terminal handlers are useful diagnostics when inspecting a
     * changed dispatch table, but they are not sufficient to grant native
     * execution. Only the exact executable hash or the retail layout does. */
    if (image_has_word(&image, AI_COMMAND_TABLE + AI_END_FIELD_INDEX * 4u)) {
        play_field_card = Memories_LoadLE32(
            image.data + (AI_COMMAND_TABLE - image.address) +
            AI_PLAY_FIELD_CARD_INDEX * 4u);
        end_hand = Memories_LoadLE32(
            image.data + (AI_COMMAND_TABLE - image.address) +
            AI_END_HAND_INDEX * 4u);
        end_field = Memories_LoadLE32(
            image.data + (AI_COMMAND_TABLE - image.address) +
            AI_END_FIELD_INDEX * 4u);
        profile->ai_play_field_card = play_field_card;
        profile->ai_end_hand = end_hand;
        profile->ai_end_field = end_field;
        profile->has_ai_signature = 1;
        /* The terminal opcodes keep their retail handlers in the images that
         * keep the layout; a community image that moved them runs its own
         * end-of-action opcodes and says so in the log. */
        profile->terminal_handlers_retail =
            play_field_card == 0x80070F1Cu && end_hand == 0x80070FF8u &&
            end_field == 0x80071000u;
    }

    return 0;
}

const char *Memories_ExecutionProfileName(MemoriesExecutionProfileId id)
{
    switch (id) {
    case MEMORIES_PROFILE_VANILLA: return "vanilla";
    case MEMORIES_PROFILE_MODIFIED: return "modified";
    case MEMORIES_PROFILE_UNKNOWN: return "unknown";
    default: return "invalid";
    }
}

const char *Memories_ExecutionModeName(MemoriesExecutionMode mode)
{
    switch (mode) {
    case MEMORIES_EXECUTION_NATIVE_HYBRID: return "native-hybrid";
    case MEMORIES_EXECUTION_FULL_GUEST: return "full-guest";
    default: return "invalid";
    }
}
