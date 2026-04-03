/* pc_acc_menu.h - Menu narration: choice windows, inventory, keyboard, clock */
#ifndef PC_ACC_MENU_H
#define PC_ACC_MENU_H

#include "m_choice.h"
#include "m_editor_ovl.h"
#include "lb_rtc.h"

/* Called when a choice menu enters NORMAL state (becomes interactive).
 * Speaks all available choices and highlights the current selection. */
void pc_acc_choice_appeared(mChoice_c* choice);

/* Called each frame during choice navigation.
 * Speaks the newly highlighted option if the cursor moved. */
void pc_acc_choice_update(mChoice_c* choice);

/* Called when the cursor moves to a new item in inventory/tag menus.
 * tag_str0 is the game-encoded item name (up to max_len bytes).
 * table is the mTG_TABLE_* value identifying which section the cursor is in. */
void pc_acc_tag_item_changed(const u8* tag_str0, int max_len, int table);

/* Called when the timeIn (clock adjustment) overlay opens.
 * Speaks the initial field and value. */
void pc_acc_timein_opened(int sel_idx, const u16* values, int month);

/* Called each frame during timeIn navigation.
 * Speaks when field selection or value changes. */
void pc_acc_timein_update(int sel_idx, const u16* values, int month);

/* Called when the keyboard/editor overlay opens. Speaks the highlighted character.
 * grid_char is the game-encoded byte at the current grid cursor position. */
void pc_acc_editor_opened(mED_Ovl_c* editor_ovl, u8 grid_char);

/* Called each frame during editor input. Speaks cursor movement, typed chars, deletions.
 * grid_char is the game-encoded byte at the current grid cursor position. */
void pc_acc_editor_update(mED_Ovl_c* editor_ovl, u8 grid_char);

/* Called when an item action popup opens (Drop/Give/Grab/etc.).
 * Speaks all action labels and marks the current one as selected. */
void pc_acc_tag_action_opened(const u8* const* action_strs, int num_actions, int sel_row, int str_len);

/* Called when cursor moves within the action popup.
 * Speaks the newly highlighted action label. */
void pc_acc_tag_action_update(const u8* action_str, int str_len);

/* Called when the inventory/tag cursor moves to a different table section.
 * table is a mTG_TABLE_* value. Speaks the section name. */
void pc_acc_tag_table_changed(int table);

/* Called when the tag overlay is constructed for a submenu.
 * menu_type is the mSM_OVL_* value. Speaks the menu name. */
void pc_acc_submenu_opened(int menu_type);

/* Called when the town map overlay opens or cursor moves.
 * Speaks acre label, building name, and resident names. */
void pc_acc_map_opened(int sel_bx, int sel_bz, int player_bx, int player_bz,
                       const u8* land_name, int land_name_len);
void pc_acc_map_cursor_moved(int sel_bx, int sel_bz, int player_bx, int player_bz,
                             int label_no, int label_cnt,
                             const u8* resident_names[], int resident_count);

/* Called when the address book opens to selection or cursor moves.
 * Speaks the currently highlighted addressee name. */
void pc_acc_address_selection(const u8* name, int name_len, int entry_idx, int entry_count, int page_idx);

/* Called when the notice board overlay opens or page changes.
 * Speaks entry number, date, and message text. */
void pc_acc_notice_opened(int now_page, int page_count,
                          const u8* message, int msg_len,
                          const lbRTC_time_c* post_time);
void pc_acc_notice_page_changed(int now_page, int page_count,
                                const u8* message, int msg_len,
                                const lbRTC_time_c* post_time);

/* Called when a warning popup appears (e.g. "You can't carry more than 99,999 Bells!").
 * warning_type is mWR_WARNING_* index, lines/num_lines are the text line data. */
void pc_acc_warning_opened(int warning_type, const u8* const* line_strs, const int* line_lens, int num_lines);

/* Called when the letter reading overlay opens (board overlay in READ mode).
 * Speaks the header, body, and footer of the letter. */
void pc_acc_board_opened(const u8* header, int header_len,
                         const u8* body, int body_len,
                         const u8* footer, int footer_len);

/* Called when the bank overlay opens. Speaks wallet and account balance. */
void pc_acc_bank_opened(int wallet, int account);

/* Called each frame during bank navigation.
 * Speaks cursor position and value changes. */
void pc_acc_bank_update(int cursol, int wallet, int account, int bell);

/* Called when the loan repayment overlay opens. Speaks loan and money info. */
void pc_acc_repay_opened(u32 money, u32 loan);

/* Called each frame during repay navigation.
 * Speaks cursor position and value changes. */
void pc_acc_repay_update(int cursor_idx, u32 repay_amount, u32 money, u32 loan);

#endif
