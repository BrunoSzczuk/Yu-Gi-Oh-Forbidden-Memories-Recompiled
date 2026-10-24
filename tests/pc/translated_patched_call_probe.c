/* The guest side of the patched-call wrapper test: a function the build knows
 * a retail address for, so the wrapper around it can ask the interpreter for
 * the image's own code. The retail body is `value + 1`. */
int probe(int value)
{
    return value + 1;
}
