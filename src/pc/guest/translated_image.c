#include "translated_image.h"
#include "code_patches.h"
#include "image_loader.h"
#include <string.h>

static uint32_t resident_base;
static uint32_t resident_end;
static MemoriesExecutionProfile resident_profile;
static int resident_profile_valid;

int Memories_TranslatedExeContains(uint32_t address)
{
    return resident_base && address >= resident_base && address < resident_end;
}

const MemoriesExecutionProfile *Memories_TranslatedExecutionProfile(void)
{
    return resident_profile_valid ? &resident_profile : NULL;
}

int Memories_LoadTranslatedExe(MemoriesMemory *memory, const uint8_t *data,
                              size_t length, uint32_t *entry)
{
    MemoriesExeImage image;
    MemoriesExecutionProfile profile;
    void *destination;
    if (!memory || !entry || Memories_ParseExe(data, length, 1, &image)) return -1;
    if (Memories_ExecutionProfileDetect(data, length, &profile)) return -1;
    destination = Memories_Resolve(memory, image.address, image.size, 4);
    if (!destination) return -1;
    memcpy(destination, image.data, image.size);
    /* Which of the retail functions the image changed: they run in the
     * interpreter, the retail ones natively. */
    Memories_CodePatchesScan(image.data, image.size, image.address);
    resident_base = image.address;
    resident_end = image.size <= UINT32_MAX - image.address ? image.address + image.size : UINT32_MAX;
    resident_profile = profile;
    resident_profile_valid = 1;
    *entry = image.entry;
    return 0;
}
