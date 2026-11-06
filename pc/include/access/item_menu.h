/* item_menu.h - Speech for the game's item menus (the "tag" overlay).
 *
 * The tag overlay (src/game/m_tag_ovl.c) drives the inventory, mailbox,
 * gyroid storage, catalog, music list, design lists and similar grids. When
 * the cursor lands on a slot, the game fills a label ("tag") with up to three
 * lines of text; that is what is spoken, with the slot's position.
 *
 * Context first, then focus: when a menu opens or the cursor moves to another
 * section, the menu or section name interrupts and the focused slot is queued
 * after it, so neither cuts the other off. */
#ifndef ACCESS_ITEM_MENU_H
#define ACCESS_ITEM_MENU_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A submenu overlay was constructed; menu_type is an mSM_OVL_* value. */
void acc_item_menu_opened(int menu_type);

/* The cursor moved into another section; table is an mTG_TABLE_* value. */
void acc_item_section_changed(int table);

/* The label for the focused slot was rebuilt. lines are the label's three
 * text lines (game encoding, padded to line_len). index is the slot number
 * within the section and count the section's slot count. */
void acc_item_slot_focused(int table, int index, int count, const u8* line0, const u8* line1, const u8* line2,
                           int line_len);

/* An action popup (Drop, Give, ...) opened with count actions. */
void acc_item_actions_opened(const u8* const* labels, int count, int selected, int label_len);

/* The highlighted action in the popup changed. */
void acc_item_action_moved(const u8* label, int label_len, int index, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_ITEM_MENU_H */
