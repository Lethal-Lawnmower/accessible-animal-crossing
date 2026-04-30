/* pc_acc_nav.h - Navigation assistance for blind players.
 *
 * Category-based object scanning with compass directions.
 * Keybinds match Pokémon Access / LADX accessibility conventions:
 *   Shift+L / Shift+J = cycle category
 *   L / J = next/prev item in category
 *   K = announce directions to selected target
 *
 * Auto-announcements for NPC proximity. */
#ifndef PC_ACC_NAV_H
#define PC_ACC_NAV_H

#include "types.h"

/* Call once per frame from the main game loop. */
void pc_acc_nav_update(void);

/* Returns 1 if accessibility nav system is active (J/K/L consumed). */
int pc_acc_nav_is_active(void);

/* Build static obstacle grid from save data (FG items, houses, elevation, BG).
 * Call after mFM_SetBlockKindLoadCombi() and FG data is finalized. */
void pc_acc_nav_build_obstacle_grid(void);

/* Auto-walk (Ctrl+K): called by pc_pad.c each frame to retrieve injected
 * stick values. Writes 0,0 when auto-walk is inactive. */
void pc_acc_nav_get_autowalk_stick(s8* sx, s8* sy);

/* Auto-walk cancel signal from pc_pad.c when the user provides manual input. */
void pc_acc_nav_autowalk_cancel_on_input(void);

#endif
