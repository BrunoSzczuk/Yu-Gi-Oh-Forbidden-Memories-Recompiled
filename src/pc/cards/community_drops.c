/* The community drop mod: a disc that rewrites the results screen's draw to
 * deal several cards a win. The kit (the community's patch kit, read by
 * tools/pc/fm_editor/kit.py) puts a jump where the retail draws the prize,
 * to a loop of its own that draws a card into a buffer as many times as a
 * limit allows, and another jump where the retail gives the card away, to a
 * loop that gives the buffer's cards away.
 *
 * The port runs its own native code there, so the kit's code never runs
 * (notes/community-images.md, "What still does not work"); this reads what
 * the disc asks for, so the port can deal the same number of cards the disc
 * would. The count is the limit less one: the loop stops when its counter
 * reaches the limit, and the counter counts from one.
 *
 * The kit's code lives in the screen packages the game loads, and the
 * disc's executable holds an older copy of it. The copy that runs at the
 * results screen is the one in the package the game loads for a duel, and
 * it is the one that counts what the disc deals; the executable's copy
 * asks for fewer cards. So the count is read every time it is asked for,
 * and the one read while the guest is in a duel is the one kept.
 */
#include "community_drops.h"
#include "pc/debug/log.h"
#include <stdint.h>
#include <string.h>

/* DuelScene_UpdateResultRewards (0x800218F0): the prize's draw, where the
 * kit puts its jump. */
#define RESULT_DRAW 0x80021C6Cu
/* Where the executable's image sits in the guest's memory, and how much of
 * it there is (the disc's executable is the retail's size). */
#define IMAGE_BASE 0x80010000u
#define IMAGE_SIZE 0x1D0000u
/* The most cards a win may deal (the range of Game > Card drops). */
#define MAX_COUNT 99
/* How many instructions of the kit's loop are read. */
#define LOOP_INSTRUCTIONS 48
/* The mode the guest is in (main_mode_state.h): its low five bits. The duel
 * is mode 3 (main_modes.h). */
#define MODE_MASK 0x1F
#define MODE_DUEL 3

extern unsigned char D_8009B26C; /* main_mode_state.h: the running mode */

typedef struct {
    uint32_t word;
    int op, rs, rt, imm;
} Ins;

static Ins ins_of(uint32_t word)
{
    Ins ins;
    ins.word = word;
    ins.op = (int)(word >> 26);
    ins.rs = (int)((word >> 21) & 31);
    ins.rt = (int)((word >> 16) & 31);
    ins.imm = (int)(int16_t)(word & 0xFFFFu);
    return ins;
}

/* The word at an address of the executable's image, or 0 when the address
 * is outside it. */
static uint32_t word_at(const unsigned char *image, size_t size, uint32_t address)
{
    uint32_t at;
    if (address < IMAGE_BASE) return 0;
    at = address - IMAGE_BASE;
    if ((size_t)at + 4 > size) return 0;
    return (uint32_t)image[at] | ((uint32_t)image[at + 1] << 8) | ((uint32_t)image[at + 2] << 16) |
           ((uint32_t)image[at + 3] << 24);
}

int CommunityDrops_CountInImage(const unsigned char *image, size_t size)
{
    uint32_t word = word_at(image, size, RESULT_DRAW), target;
    int limit_of[32], counter[32], i, count = 0;

    /* The retail's own code is not a jump: the disc does not patch the drop. */
    if ((word >> 26) != 2) return 0;
    target = ((RESULT_DRAW + 4) & 0xF0000000u) | ((word & 0x03FFFFFFu) << 2);
    if (target < IMAGE_BASE || target >= IMAGE_BASE + size) return 0;

    /* The kit's loop counts the draws in a byte and compares the counter
     * against a limit: the limit is an immediate (addiu $r, $zero, n), the
     * counter the byte the loop keeps (lbu) or the register it adds one to. */
    memset(limit_of, 0, sizeof(limit_of));
    memset(counter, 0, sizeof(counter));
    for (i = 0; i < LOOP_INSTRUCTIONS; i++) {
        Ins ins = ins_of(word_at(image, size, target + 4 * (uint32_t)i));
        if (ins.op == 9 && ins.rs == 0 && ins.rt != 0 && ins.imm >= 2 && ins.imm <= MAX_COUNT) {
            limit_of[ins.rt] = ins.imm;
            counter[ins.rt] = 0;
            continue;
        }
        if (ins.op == 9 && ins.rs == ins.rt && ins.imm == 1) counter[ins.rt] = 1;
        if (ins.op == 32 || ins.op == 36) counter[ins.rt] = 1;
        if (ins.op == 4) { /* beq $rs, $rt */
            int limit = 0;
            if (limit_of[ins.rs] && counter[ins.rt]) limit = limit_of[ins.rs];
            else if (limit_of[ins.rt] && counter[ins.rs]) limit = limit_of[ins.rt];
            if (limit) {
                count = limit - 1;
                break;
            }
        }
    }
    return count;
}

int CommunityDrops_Count(void)
{
    /* The count the results screen's code asks for, read from the guest's
     * memory: the copy of the kit's loop that is loaded right now. Outside a
     * duel that is the executable's own copy, which asks for fewer cards than
     * the disc deals, so it is only read while the guest is in a duel and
     * then kept. */
    static int in_duel;
    int count = CommunityDrops_CountInImage((const unsigned char *)(uintptr_t)IMAGE_BASE, IMAGE_SIZE);
    if (count > 0 && (D_8009B26C & MODE_MASK) == MODE_DUEL) {
        if (count != in_duel) {
            in_duel = count;
            LOG(LOG_MODS, "drops: the disc's results screen deals %d cards a win", count);
        }
    }
    return in_duel;
}
