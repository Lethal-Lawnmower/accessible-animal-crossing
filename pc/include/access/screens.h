/* screens.h - Speech for the game's smaller single-purpose menu screens:
 * clock setting, bank, loan repayment and warning popups.
 *
 * Each screen has an "opened" call (announce the screen and the focus) and,
 * where the player edits values, an "update" call made every frame that
 * speaks only when the focus or a value changed. */
#ifndef ACCESS_SCREENS_H
#define ACCESS_SCREENS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clock (src/game/m_timeIn_ovl.c). values are the mTI_IDX_* fields as the
 * screen stores them (year as an offset from GAME_YEAR_MIN). */
void acc_clock_opened(int selected, const u16* values);
void acc_clock_update(int selected, const u16* values);

/* Bank (src/game/m_bank_ovl.c). cursor 0..5 are digit columns from
 * hundred-thousands to ones, 6 is OK. */
void acc_bank_opened(int wallet, int account);
void acc_bank_update(int cursor, int wallet, int account, int start_wallet);

/* Loan repayment (src/game/m_repay_ovl.c). Same cursor layout as the bank. */
void acc_repay_opened(u32 money, u32 loan);
void acc_repay_update(int cursor, u32 amount, u32 money, u32 loan);

/* Birthday entry (src/game/m_birthday_ovl.c). Call opened once, then update
 * every frame. field is mBR_Ovl_IDX_* (month, day, OK). */
void acc_birthday_opened(void);
void acc_birthday_update(int field, int month, int day);

/* "Is this OK?" after writing (src/game/m_editEndChk_ovl.c). Call opened
 * once, then update every frame. listing is TRUE once the answers show. */
void acc_confirm_opened(const u8* question, int question_len);
void acc_confirm_update(int listing, int selected, const u8* const* answers, const int* lengths, int count);

/* Town tune editor (src/game/m_mscore_ovl.c). cursor 0..15 is a note slot,
 * 16 is the finish button. clearing is TRUE while the "clear all" Yes/No
 * box is up, with clear_choice 0 = Yes, 1 = No. */
void acc_tune_opened(void);
void acc_tune_update(int cursor, const u8* melody);
void acc_tune_clear_update(int clearing, int clear_choice);

/* Town map (src/game/m_map_ovl.c), called every frame while the map is
 * interactive. block_text is what the map shows for the selected acre (its
 * label words or resident names, already UTF-8). Rows are lettered from the
 * top (A) and columns numbered from the left (1). */
void acc_map_opened(const char* town_name);
void acc_map_update(int col, int row, int player_col, int player_row, const char* block_text);

/* A popup made of text lines (warnings, memory card notices). */
void acc_popup_lines(const u8* const* lines, const int* lengths, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_SCREENS_H */
