/* pc_acc_tree.h - Tree interaction and balloon present accessibility.
 *
 * Trees: proximity announcements (type, contents), shake result TTS,
 *        wasp warnings, empty tree notification.
 * Balloons: spawn/nearby/stuck/escape/present-drop announcements. */
#ifndef PC_ACC_TREE_H
#define PC_ACC_TREE_H

#include "types.h"
#include "m_actor_type.h"

/* Per-frame update — call from main loop. Handles tree proximity scanning
 * and balloon state tracking. */
void pc_acc_tree_update(void);

/* Tree shake hook — called from Player_actor_SetEffect_Shake_tree()
 * at animation frame 10 when the shake registers.
 * item = the tree's FG item ID before the shake. */
void pc_acc_tree_shaken(mActor_name_t item, int ut_x, int ut_z);

/* Tree drop hook — called from drop_fruit() after items are spawned.
 * dropped_item = what actually fell, count = how many. */
void pc_acc_tree_drop(mActor_name_t dropped_item, int count);

/* Balloon spawn hook — called from Balloon_make_fuusen() on successful spawn. */
void pc_acc_balloon_spawned(void);

/* Balloon stuck hook — called from aFSN_wood_stop_init() when balloon sticks to tree. */
void pc_acc_balloon_stuck(f32 bx, f32 bz);

/* Balloon escape hook — called from aFSN_escape_init().
 * from_tree = 1 if escaping from a tree (present may drop), 0 if edge/boundary escape. */
void pc_acc_balloon_escaping(int from_tree, f32 bx, f32 bz);

/* Balloon present dropped hook — called from aFSN_escape() when ITM_PRESENT is placed. */
void pc_acc_balloon_present_dropped(f32 px, f32 pz);

#endif
