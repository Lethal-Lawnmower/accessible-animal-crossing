/* screens.c - Smaller menu screens. See access/screens.h.
 *
 * Carried over from the previous project's pc_acc_menu.c, checked against
 * the current source:
 *  - Clock: fields are hour (0-23 as displayed), minute, month, day, year
 *    (stored as an offset from GAME_YEAR_MIN, 2000). The weekday the screen
 *    shows is now announced with the date.
 *  - Bank and loan: cursor 0..5 are digits worth 100,000 down to 1, 6 is OK
 *    (mBN_cursol_2_keta, step_money in m_repay_ovl.c). Changes now say
 *    whether bells are being deposited or withdrawn, and digit focus reads as
 *    "100,000s digit" with the amount, rather than a bare place name. */

#include "access/screens.h"

#include "access/game_text.h"
#include "access/speech.h"

#include "lb_rtc.h"
#include "m_timeIn_ovl.h"

#include <stdio.h>
#include <string.h>

/* ---- Number formatting --------------------------------------------------- */

/* 1234567 -> "1,234,567" */
static void group_digits(unsigned long value, char* out, int out_size) {
    char raw[24];
    int len = snprintf(raw, sizeof(raw), "%lu", value);
    int pos = 0;
    for (int i = 0; i < len && pos < out_size - 1; i++) {
        if (i > 0 && (len - i) % 3 == 0 && pos < out_size - 1) {
            out[pos++] = ',';
        }
        out[pos++] = raw[i];
    }
    out[pos] = '\0';
}

static const char* const s_digit_names[6] = {
    "100,000s", "10,000s", "1,000s", "100s", "10s", "1s",
};

/* ---- Clock --------------------------------------------------------------- */

static const char* const s_months[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};

static const char* const s_weekdays[lbRTC_WEEK] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};

static int s_clock_selected = -1;
static u16 s_clock_values[mTI_IDX_NUM];

static void clock_field(int selected, const u16* values, char* out, int out_size) {
    int year = GAME_YEAR_MIN + values[mTI_IDX_YEAR];
    int month = values[mTI_IDX_MONTH];
    int day = values[mTI_IDX_DAY];
    const char* weekday = "";
    int wd = lbRTC_Week((lbRTC_year_t)year, (lbRTC_month_t)month, (lbRTC_day_t)day);
    if (wd >= 0 && wd < lbRTC_WEEK) {
        weekday = s_weekdays[wd];
    }

    switch (selected) {
        case mTI_IDX_HOUR:
            snprintf(out, out_size, "Hour %d", values[mTI_IDX_HOUR]);
            break;
        case mTI_IDX_MIN:
            snprintf(out, out_size, "Minute %d", values[mTI_IDX_MIN]);
            break;
        case mTI_IDX_MONTH:
            if (month >= 1 && month <= 12) {
                snprintf(out, out_size, "Month %s, %s", s_months[month - 1], weekday);
            } else {
                snprintf(out, out_size, "Month %d", month);
            }
            break;
        case mTI_IDX_DAY:
            snprintf(out, out_size, "Day %d, %s", day, weekday);
            break;
        case mTI_IDX_YEAR:
            snprintf(out, out_size, "Year %d, %s", year, weekday);
            break;
        case mTI_IDX_OK:
            snprintf(out, out_size, "OK");
            break;
        default:
            out[0] = '\0';
            break;
    }
}

void acc_clock_opened(int selected, const u16* values) {
    char field[64];
    char text[128];
    clock_field(selected, values, field, sizeof(field));
    snprintf(text, sizeof(text), "Set the clock. %s", field);
    acc_speak_interrupt(text);
    s_clock_selected = selected;
    memcpy(s_clock_values, values, sizeof(s_clock_values));
}

void acc_clock_update(int selected, const u16* values) {
    char field[64];
    if (selected == s_clock_selected && memcmp(values, s_clock_values, sizeof(s_clock_values)) == 0) {
        return;
    }
    clock_field(selected, values, field, sizeof(field));
    acc_speak_interrupt(field);
    s_clock_selected = selected;
    memcpy(s_clock_values, values, sizeof(s_clock_values));
}

/* ---- Bank ---------------------------------------------------------------- */

static int s_bank_cursor = -1;
static int s_bank_wallet = -1;

void acc_bank_opened(int wallet, int account) {
    char w[24], a[24], text[128];
    group_digits((unsigned long)wallet, w, sizeof(w));
    group_digits((unsigned long)account, a, sizeof(a));
    snprintf(text, sizeof(text), "Bank. Wallet %s bells, account %s bells.", w, a);
    acc_speak_interrupt(text);
    s_bank_cursor = -1;
    s_bank_wallet = wallet;
}

void acc_bank_update(int cursor, int wallet, int account, int start_wallet) {
    char text[160];
    if (wallet != s_bank_wallet) {
        char moved[24], w[24], a[24];
        int diff = wallet - start_wallet;
        group_digits((unsigned long)(diff < 0 ? -diff : diff), moved, sizeof(moved));
        group_digits((unsigned long)wallet, w, sizeof(w));
        group_digits((unsigned long)account, a, sizeof(a));
        if (diff == 0) {
            snprintf(text, sizeof(text), "No change. Wallet %s, account %s.", w, a);
        } else {
            snprintf(text, sizeof(text), "%s %s. Wallet %s, account %s.", diff < 0 ? "Deposit" : "Withdraw", moved,
                     w, a);
        }
        acc_speak_interrupt(text);
    } else if (cursor != s_bank_cursor) {
        if (cursor == 6) {
            acc_speak_interrupt("OK");
        } else if (cursor >= 0 && cursor < 6) {
            snprintf(text, sizeof(text), "%s digit", s_digit_names[cursor]);
            acc_speak_interrupt(text);
        }
    }
    s_bank_cursor = cursor;
    s_bank_wallet = wallet;
}

/* ---- Loan repayment ------------------------------------------------------ */

static int s_repay_cursor = -1;
static u32 s_repay_amount = 0xFFFFFFFFu;

void acc_repay_opened(u32 money, u32 loan) {
    char m[24], l[24], text[128];
    group_digits(money, m, sizeof(m));
    group_digits(loan, l, sizeof(l));
    snprintf(text, sizeof(text), "Pay off loan. You have %s bells, loan %s bells.", m, l);
    acc_speak_interrupt(text);
    s_repay_cursor = -1;
    s_repay_amount = 0;
}

void acc_repay_update(int cursor, u32 amount, u32 money, u32 loan) {
    char text[160];
    if (amount != s_repay_amount) {
        char a[24], m[24], l[24];
        group_digits(amount, a, sizeof(a));
        group_digits(money, m, sizeof(m));
        group_digits(loan, l, sizeof(l));
        snprintf(text, sizeof(text), "Pay %s. Loan left %s, bells left %s.", a, l, m);
        acc_speak_interrupt(text);
    } else if (cursor != s_repay_cursor) {
        if (cursor == 6) {
            acc_speak_interrupt("OK");
        } else if (cursor >= 0 && cursor < 6) {
            snprintf(text, sizeof(text), "%s digit", s_digit_names[cursor]);
            acc_speak_interrupt(text);
        }
    }
    s_repay_cursor = cursor;
    s_repay_amount = amount;
}

/* ---- Birthday ------------------------------------------------------------ */

static int s_bday_field = -1;
static int s_bday_month = -1;
static int s_bday_day = -1;
static int s_bday_fresh = FALSE;

void acc_birthday_opened(void) {
    s_bday_fresh = TRUE;
    s_bday_field = s_bday_month = s_bday_day = -1;
}

void acc_birthday_update(int field, int month, int day) {
    char text[96];
    if (!s_bday_fresh && field == s_bday_field && month == s_bday_month && day == s_bday_day) {
        return;
    }
    switch (field) {
        case 0: /* mBR_Ovl_IDX_MONTH */
            snprintf(text, sizeof(text), "Month %s", (month >= 1 && month <= 12) ? s_months[month - 1] : "unknown");
            break;
        case 1: /* mBR_Ovl_IDX_DAY */
            snprintf(text, sizeof(text), "Day %d", day);
            break;
        default:
            snprintf(text, sizeof(text), "OK, %s %d", (month >= 1 && month <= 12) ? s_months[month - 1] : "", day);
            break;
    }
    if (s_bday_fresh) {
        char intro[128];
        snprintf(intro, sizeof(intro), "Birthday. %s", text);
        acc_speak_interrupt(intro);
        s_bday_fresh = FALSE;
    } else {
        acc_speak_interrupt(text);
    }
    s_bday_field = field;
    s_bday_month = month;
    s_bday_day = day;
}

/* ---- "Is this OK?" ------------------------------------------------------- */

static int s_confirm_listing = -1;
static int s_confirm_selected = -1;

void acc_confirm_opened(const u8* question, int question_len) {
    char text[64];
    acc_game_field_to_utf8(question, question_len, text, sizeof(text));
    acc_speak_interrupt(text[0] ? text : "Is this OK?");
    s_confirm_listing = FALSE;
    s_confirm_selected = -1;
}

void acc_confirm_update(int listing, int selected, const u8* const* answers, const int* lengths, int count) {
    char label[48];
    char text[96];
    if (!listing) {
        s_confirm_listing = FALSE;
        return;
    }
    if (s_confirm_listing && selected == s_confirm_selected) {
        return;
    }
    if (selected < 0 || selected >= count) {
        return;
    }
    acc_game_field_to_utf8(answers[selected], lengths[selected], label, sizeof(label));
    snprintf(text, sizeof(text), "%s, %d of %d", label, selected + 1, count);
    acc_speak_interrupt(text);
    s_confirm_listing = TRUE;
    s_confirm_selected = selected;
}

/* ---- Town tune ------------------------------------------------------------ */

/* Note values 0..12 follow the editor's own sound table (single_se in
 * m_mscore_ovl.c): NA_SE_NOTE_G_LOW .. NA_SE_NOTE_E. 13 plays a random note,
 * 14 holds the previous note ("hold" in that table; the clear-all action
 * fills every slot with it), 15 is silence ("off"). The previous project
 * called 14 "rest". */
static const char* const s_tune_notes[16] = {
    "low G", "low A", "low B", "low C", "low D", "low E", "low F",
    "G", "A", "B", "C", "D", "E", "random", "hold", "off",
};

static int s_tune_fresh = FALSE;
static int s_tune_cursor = -2;
static u8 s_tune_melody[16];
static int s_tune_clearing = FALSE;
static int s_tune_choice = -1;

void acc_tune_opened(void) {
    s_tune_fresh = TRUE;
    s_tune_cursor = -2;
    s_tune_clearing = FALSE;
}

static void tune_slot_text(int cursor, const u8* melody, char* out, int out_size) {
    if (cursor >= 0 && cursor < 16) {
        int note = melody[cursor] < 16 ? melody[cursor] : 15;
        snprintf(out, out_size, "%d, %s", cursor + 1, s_tune_notes[note]);
    } else if (cursor == 16) {
        snprintf(out, out_size, "Finish");
    } else {
        out[0] = '\0';
    }
}

void acc_tune_update(int cursor, const u8* melody) {
    char slot[48];
    char text[160];
    if (cursor < 0) {
        return;
    }
    if (!s_tune_fresh && cursor == s_tune_cursor && memcmp(melody, s_tune_melody, 16) == 0) {
        return;
    }
    tune_slot_text(cursor, melody, slot, sizeof(slot));
    if (s_tune_fresh) {
        snprintf(text, sizeof(text), "Town tune, 16 notes. Up and down change the note, X plays, Y clears, Start saves. %s",
                 slot);
        acc_speak_interrupt(text);
        s_tune_fresh = FALSE;
    } else if (cursor == s_tune_cursor && cursor >= 0 && cursor < 16) {
        /* Same slot, new note: just the note. */
        acc_speak_interrupt(s_tune_notes[melody[cursor] < 16 ? melody[cursor] : 15]);
    } else {
        acc_speak_interrupt(slot);
    }
    s_tune_cursor = cursor;
    memcpy(s_tune_melody, melody, 16);
}

void acc_tune_clear_update(int clearing, int clear_choice) {
    if (!clearing) {
        s_tune_clearing = FALSE;
        return;
    }
    if (s_tune_clearing && clear_choice == s_tune_choice) {
        return;
    }
    if (!s_tune_clearing) {
        acc_speak_interrupt("Clear all notes?");
        acc_speak_queue(clear_choice == 0 ? "Yes" : "No");
    } else {
        acc_speak_interrupt(clear_choice == 0 ? "Yes" : "No");
    }
    s_tune_clearing = TRUE;
    s_tune_choice = clear_choice;
}

/* ---- Popups -------------------------------------------------------------- */

void acc_popup_lines(const u8* const* lines, const int* lengths, int count) {
    char text[768];
    int pos = 0;
    text[0] = '\0';
    for (int i = 0; i < count; i++) {
        char line[256];
        if (!lines[i] || lengths[i] <= 0) {
            continue;
        }
        acc_game_field_to_utf8(lines[i], lengths[i], line, sizeof(line));
        if (line[0] == '\0') {
            continue;
        }
        if (pos > 0) {
            acc_text_append(text, sizeof(text), &pos, " ");
        }
        acc_text_append(text, sizeof(text), &pos, line);
    }
    if (pos > 0) {
        acc_speak_interrupt(text);
    }
}
