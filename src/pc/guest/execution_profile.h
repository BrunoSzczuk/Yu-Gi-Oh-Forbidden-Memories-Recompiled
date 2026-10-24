#ifndef MEMORIES_GUEST_EXECUTION_PROFILE_H
#define MEMORIES_GUEST_EXECUTION_PROFILE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MEMORIES_PROFILE_VANILLA = 0,
    MEMORIES_PROFILE_MODIFIED,
    MEMORIES_PROFILE_UNKNOWN,
} MemoriesExecutionProfileId;

typedef enum {
    MEMORIES_EXECUTION_NATIVE_HYBRID = 0,
    MEMORIES_EXECUTION_FULL_GUEST,
} MemoriesExecutionMode;

typedef struct {
    MemoriesExecutionProfileId id;
    MemoriesExecutionMode mode;
    uint8_t executable_sha256[32];
    uint32_t load_address;
    uint32_t image_size;
    uint32_t entry;
    uint32_t ai_play_field_card;
    uint32_t ai_end_hand;
    uint32_t ai_end_field;
    unsigned has_ai_signature;
    unsigned has_vanilla_hash;
    unsigned layout_compatible;
    unsigned terminal_handlers_retail;
} MemoriesExecutionProfile;

/* Classify a PS-X EXE without touching guest memory. A valid executable whose
 * layout is not the known vanilla layout is deliberately classified as
 * FULL_GUEST rather than being guessed as vanilla. */
int Memories_ExecutionProfileDetect(const uint8_t *data, size_t length,
                                    MemoriesExecutionProfile *profile);

const char *Memories_ExecutionProfileName(MemoriesExecutionProfileId id);
const char *Memories_ExecutionModeName(MemoriesExecutionMode mode);

#endif
