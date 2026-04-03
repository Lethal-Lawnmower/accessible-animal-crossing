/* pc_acc_magnet.h - Magnet pickup system for accessibility.
 *
 * Auto-collects items that the player deliberately generated
 * (tree shakes, rock hits, etc.) without requiring precise navigation.
 *
 * Toggle: F4 key
 * Announces each collected item via TTS. */
#ifndef PC_ACC_MAGNET_H
#define PC_ACC_MAGNET_H

#include "types.h"
#include "m_name_table.h"

/* Call once per frame from the main game loop. */
void pc_acc_magnet_update(void);

/* Called by the drop system when an item lands on the FG grid.
 * This registers the item for automatic collection. */
void pc_acc_magnet_notify_drop(mActor_name_t item, f32 x, f32 y, f32 z);

#endif
