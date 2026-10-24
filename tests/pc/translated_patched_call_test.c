/* The host side of the patched-call wrapper test.
 *
 * A community image may have replaced the code at a retail function's
 * address. The native call to that address then runs the image's own code,
 * in the interpreter, and falls back to the retail code when the interpreter
 * cannot run it. Memories_MipsTry answers 0 when the image's code ran and a
 * non-zero status when it could not: the wrapper must keep the interpreter's
 * answer in the first case and the retail answer in the second.
 *
 * The retail body is reached through Hooks_Resolve, so the number of
 * Hooks_Resolve calls is the number of times the native code ran. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

extern int probe(int value);

static int patched;
static int interpreter_status;
static uint32_t interpreter_value;
static int native_calls;
static int interpreter_calls;

int Memories_CodePatchedCall(uint32_t address)
{
    (void)address;
    return patched;
}

void *Hooks_Resolve(void *function, void *body)
{
    (void)function;
    native_calls++;
    return body;
}

int Memories_MipsTry(uint32_t address, const uint32_t *args, unsigned count,
                     uint32_t *result)
{
    (void)address;
    (void)args;
    (void)count;
    interpreter_calls++;
    if (interpreter_status) return interpreter_status;
    *result = interpreter_value;
    return 0;
}

int main(void)
{
    /* An image that changed the function: the image's own code answers. */
    patched = 1;
    interpreter_status = 0;
    interpreter_value = 0x15u;
    native_calls = 0;
    interpreter_calls = 0;
    assert(probe(41) == 0x15);
    assert(interpreter_calls == 1);
    assert(native_calls == 0);

    /* The interpreter could not run the image's code: the retail code does. */
    interpreter_status = -1;
    native_calls = 0;
    assert(probe(41) == 42);
    assert(interpreter_calls == 2);
    assert(native_calls == 1);

    /* An image that did not change the function: the retail code runs. */
    patched = 0;
    native_calls = 0;
    assert(probe(41) == 42);
    assert(interpreter_calls == 2);
    assert(native_calls == 1);

    printf("patched call wrapper: ok\n");
    return 0;
}
