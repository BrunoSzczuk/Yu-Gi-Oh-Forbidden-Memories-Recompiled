/* The image profile the card tests ask cards.c for.
 *
 * cards.c reads the profile of the executable the disc was loaded with, to
 * tell a retail disc from a community image (an image says nothing about
 * the type of the card an effect belongs to). The card tests run without an
 * image, so the profile is the retail one: NULL, as a disc that was never
 * classified gives. */
#include "pc/guest/execution_profile.h"

const MemoriesExecutionProfile *Memories_TranslatedExecutionProfile(void)
{
    return NULL;
}
