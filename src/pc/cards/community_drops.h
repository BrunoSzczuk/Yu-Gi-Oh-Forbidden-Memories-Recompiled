#ifndef MEMORIES_PC_COMMUNITY_DROPS_H
#define MEMORIES_PC_COMMUNITY_DROPS_H
#include <stddef.h>

/* The community drop mod: a disc that rewrites the results screen's draw to
 * deal several cards a win (notes/card-drops.md, "The community drop mod").
 *
 * CommunityDrops_Count: how many cards the loaded disc draws a win, or 0
 * when it does not patch the drop (the retail's own single card). The count
 * is the one the disc's results screen asks for, which the port knows once
 * a duel has run (the copy of the code that runs is in the screen package
 * the game loads for a duel).
 *
 * CommunityDrops_CountInImage: the same, over the executable's bytes as the
 * disc has them. The guest's memory holds them at their load address, so
 * that is what the port reads; a test hands them in.
 */
int CommunityDrops_Count(void);
int CommunityDrops_CountInImage(const unsigned char *image, size_t size);

#endif
