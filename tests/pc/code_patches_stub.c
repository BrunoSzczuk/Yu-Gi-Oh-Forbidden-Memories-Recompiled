/* The tables the build generates from config/slus_01411/code_hashes.json and
 * from config/slus_01411/functions.csv (tools/pc/code_hashes.py and
 * tools/pc/code_regions.py). A test that links the guest backend outside a
 * build needs them too: these are the empty ones, so no image is seen as
 * having changed a function. */
#include "pc/guest/code_patches.h"

const MemoriesCodeHash Memories_CodeHashes[] = {{0u, 0u, {0}}};
const unsigned Memories_CodeHashCount = 0u;

const MemoriesCodeRegion Memories_CodeRegions[] = {{0u, 0u}};
const unsigned Memories_CodeRegionCount = 0u;
