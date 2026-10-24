/* A test-only stand-in for the save state's chunk writer (src/pc/guest/state.c),
 * which pulls in ucontext and the rest of the state machinery that this test
 * does not need. The guest I/O device keeps its own state instead. */
#include "pc/guest/state.h"

int Memories_StateChunk(MemoriesState *state, const char *tag,
                        const MemoriesStateField *fields, size_t count)
{
    (void)state;
    (void)tag;
    (void)fields;
    (void)count;
    return 0;
}
