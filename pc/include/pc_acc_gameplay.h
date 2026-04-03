/* pc_acc_gameplay.h - Accessibility for fishing, bug catching, fossil digging.
 *
 * Fishing: TTS cues for nibble vs bite (critical — no audio distinction in game)
 * Bug catching: continuous insect scanning, patience-based alerts, auto-approach
 * Fossil digging: SHINE_SPOT nav targets, buried item hotkey, dig result TTS */
#ifndef PC_ACC_GAMEPLAY_H
#define PC_ACC_GAMEPLAY_H

#include "types.h"

/* Per-frame update — call from main loop.
 * Handles bug scanning, auto-approach, fossil hotkey (F9). */
void pc_acc_gameplay_update(void);

/* Fishing hooks — called from ac_uki_move.c_inc */
void pc_acc_fishing_cast(void);
void pc_acc_fishing_nibble(void);
void pc_acc_fishing_bite(void);
void pc_acc_fishing_catch(int gyo_type);
void pc_acc_fishing_escape(void);

/* Bug catching hooks — called from m_player_main_swing_net.c_inc */
void pc_acc_bug_caught(u16 item);
void pc_acc_bug_missed(void);

/* Fossil digging hooks — called from m_player_main_get_scoop.c_inc */
void pc_acc_dig_unearthed(u16 item);

/* Notify that a scene/day change happened — rescan SHINE_SPOTs */
void pc_acc_gameplay_refresh_buried(void);

/* Auto-catch input injection — queried by pc_pad.c each frame.
 * Returns non-zero stick values when auto-approach is steering the player,
 * and sets press_a=1 when auto-catch wants to swing the net. */
void pc_acc_gameplay_get_auto_input(s8* stick_x, s8* stick_y, int* press_a);

#endif
