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

/* A popup made of text lines (warnings, memory card notices). */
void acc_popup_lines(const u8* const* lines, const int* lengths, int count);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_SCREENS_H */
