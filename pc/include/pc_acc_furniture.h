/* pc_acc_furniture.h - Furniture Mode: room scanning, push/pull narration,
 * rotation announcements, and blocked-move feedback for blind players. */
#ifndef PC_ACC_FURNITURE_H
#define PC_ACC_FURNITURE_H

#include "types.h"

/* Per-frame update: handles hotkey toggle, virtual cursor input, etc.
 * Called from padmgr alongside other accessibility updates. */
void pc_acc_furniture_update(void);

/* Returns 1 if Furniture Mode cursor is active (arrow keys consumed). */
int pc_acc_furniture_mode_active(void);

/* Called after a successful furniture push or pull.
 * ftr_name: item ID of the moved furniture.
 * shape: aFTR_SHAPE_TYPE* value.
 * direction: aMR_DIRECT_* value of the movement.
 * ut_x, ut_z: new grid position after the move.
 * layer: 0=floor, 1=surface. */
void pc_acc_furniture_moved(u16 ftr_name, u8 shape, int direction, int ut_x, int ut_z, int layer);

/* Called when a push or pull is blocked.
 * direction: aMR_DIRECT_* value of the attempted move.
 * ftr_name: item being moved.
 * target_ut_x, target_ut_z: the blocked destination. */
void pc_acc_furniture_blocked(int direction, u16 ftr_name, int target_ut_x, int target_ut_z);

/* Called after furniture rotation completes.
 * ftr_name: item ID, new_shape: aFTR_SHAPE_TYPE* after rotation. */
void pc_acc_furniture_rotated(u16 ftr_name, u8 new_shape);

#endif
