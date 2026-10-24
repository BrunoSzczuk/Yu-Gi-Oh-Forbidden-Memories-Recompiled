#ifndef MEMORIES_TRANSLATED_IMAGE_H
#define MEMORIES_TRANSLATED_IMAGE_H
#include "pc/memory.h"
#include "execution_profile.h"
/* Experimental translated-memory loader. Does not start the game or replace
 * the fixed-address runtime. On failure neither RAM nor entry is modified. */
int Memories_LoadTranslatedExe(MemoriesMemory *memory, const uint8_t *data,
                              size_t length, uint32_t *entry);
/* Whether address belongs to the successfully loaded resident PS-X EXE. */
int Memories_TranslatedExeContains(uint32_t address);
/* Profile selected by the last successful executable load. */
const MemoriesExecutionProfile *Memories_TranslatedExecutionProfile(void);
#endif
