/* dialogue.h - Speak message-window text (NPC dialogue, signs, system text).
 *
 * Each page of a message is spoken as soon as it starts, so the player does
 * not wait for the letter-by-letter display. Text the game inserts (player
 * name, villager name and catchphrase, items, dates, town and island names,
 * letters) is filled in with the game's own functions on a private copy of
 * the message, so wording, articles and capitals match what is displayed.
 *
 * One insertion cannot be read ahead: a random two-digit number, whose value
 * comes from the game's random number generator. Pages containing it are
 * spoken when the game has finished displaying them instead. */
#ifndef ACCESS_DIALOGUE_H
#define ACCESS_DIALOGUE_H

#include "m_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A message finished loading into its window and is about to appear.
 * new_window is TRUE when the window itself just opened (speaker is named),
 * FALSE when a follow-up message replaced the text in an open window. */
void acc_dialogue_message_loaded(mMsg_Window_c* msg_p, int new_window);

/* The player advanced to the next page; the page starts at start_idx. */
void acc_dialogue_page_advanced(mMsg_Window_c* msg_p, int start_idx);

/* The game finished displaying a page that ends at end_idx (a button prompt
 * or the end of the message). Speaks a deferred page, if any. */
void acc_dialogue_page_shown(mMsg_Window_c* msg_p, int end_idx);

/* Speak the current page again (repeat hotkey). */
void acc_dialogue_repeat(void);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_DIALOGUE_H */
