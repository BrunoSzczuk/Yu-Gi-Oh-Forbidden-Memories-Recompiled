/* A message box whose text will never end, and the wait for it
 * (message_wait.h, notes/text-control-bytecode.md).
 *
 * The name entry shows the disc's own text, and a community image's text is
 * its own: the string the retail code asks for by id is the mod's string at
 * that id, and it may have no end of its own. The box then prints its
 * letters and never finishes, and the game waits for a text that will never
 * be done.
 *
 * The retail's own text ends: the box posts its end and waits for the
 * player's press, as it always did. Only a box that has printed all it will
 * print and has not ended is given up on, so the retail's own wait is left
 * as it was. */
#include "message_wait.h"

/* Whether the player has asked for the message to go on; the frames since
 * they did; the frames in a row the box has printed nothing; the letters the
 * box had printed when it was last seen printing. */
static int asked;
static unsigned since;
static unsigned waited;
static int printed;

void MessageWait_Begin(void)
{
    asked = 0;
    since = 0;
    waited = 0;
    printed = 0;
}

int MessageWait_Frame(int asked_now, int printed_now, int ended)
{
    /* The box ended its text: the retail's own wait for the player's press
     * goes on as it always did. */
    if (ended) return 0;

    /* The player asked for the message to go on. A message the player is
     * still reading is left alone. */
    if (asked_now) {
        asked = 1;
        since = 0;
    }
    if (asked == 0) return 0;
    since++;

    /* A letter came: the box is still printing, so the wait goes on. */
    if (printed_now != printed) {
        printed = printed_now;
        waited = 0;
    } else {
        waited++;
    }

    /* The box printed nothing for two seconds: it will print nothing more,
     * and the game goes on without it. */
    if (waited >= MESSAGE_WAIT_FRAMES) return 1;

    /* The wait has lasted thirty seconds: the game goes on. */
    if (since >= MESSAGE_WAIT_LIMIT) return 1;
    return 0;
}
