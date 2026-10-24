#ifndef MEMORIES_PC_GUEST_CODE_PATCHES_H
#define MEMORIES_PC_GUEST_CODE_PATCHES_H
#include <stddef.h>
#include <stdint.h>

/* A retail function's code, by its hash. The table is the build's
 * (config/slus_01411/code_hashes.json, written by
 * tools/project/code_hashes.py): it says what the retail code at each
 * function's address looks like, so that an image which changed a function
 * can be told apart from one that did not. */
typedef struct MemoriesCodeHash {
    uint32_t address;
    uint32_t size;
    uint8_t sha256[32];
} MemoriesCodeHash;

/* The build's table. A build without one has none of these functions. */
extern const MemoriesCodeHash Memories_CodeHashes[];
extern const unsigned Memories_CodeHashCount;

/* A retail function's place in the executable: where it starts and how long
 * it is (config/slus_01411/functions.csv). The table needs no retail bytes:
 * the retail function's own shape is enough to tell an image's code there
 * from the retail code. */
typedef struct MemoriesCodeRegion {
    uint32_t address;
    uint32_t size;
} MemoriesCodeRegion;

extern const MemoriesCodeRegion Memories_CodeRegions[];
extern const unsigned Memories_CodeRegionCount;

/* Hash the loaded image's functions and mark the ones it changed. The
 * image is the executable's bytes; `load_address` is where they load. */
void Memories_CodePatchesScan(const uint8_t *image, size_t length, uint32_t load_address);
void Memories_CodePatchesReset(void);

/* Whether the function at `address` is one the image changed. Only the
 * function's own address answers: a call into the middle of a function is
 * the image's code either way. */
int Memories_CodePatched(uint32_t address);

/* Whether the native code's own call to this address has to run the image's
 * code instead of the retail code. */
int Memories_CodePatchedCall(uint32_t address);
unsigned Memories_CodePatchedCount(void);

/* The addresses of the functions the image changed, in the order they were
 * found. The count is the number of them. */
const uint32_t *Memories_CodePatchedAddresses(void);

#endif
