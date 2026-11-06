/* calendar.c - Calendar and diary speech. See access/calendar.h.
 *
 * Neither screen had speech in the previous project. Everything here comes
 * from src/game/m_calendar_ovl.c and src/game/m_diary_ovl.c:
 *  - month index _1056 counts months from January of last year (three years
 *    are built, centred on this year);
 *  - day cells hold the day number in the low 5 bits and the cell type
 *    (mCD_DAY_CURRENT for today) in the top 3;
 *  - the icon per cell is 1 for a day the player played (played_days) and 2
 *    for a town event the player took part in (event_days / event_flags). */

#include "access/calendar.h"

#include "access/game_text.h"
#include "access/speech.h"

#include <stdio.h>
#include <string.h>

static const char* const s_month_names[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December",
};

static const char* const s_weekday_names[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};

static AccCalendarState s_last;
static int s_active;
static int s_returning; /* back from the diary: speak the day, not the title */
static char s_owner[48];

static const char* month_name(int month) {
    return (month >= 1 && month <= 12) ? s_month_names[month - 1] : "unknown month";
}

static const char* ordinal_suffix(int n) {
    if (n % 100 >= 11 && n % 100 <= 13) {
        return "th";
    }
    switch (n % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

static void event_text(const AccCalendarState* s, char* out, int out_size) {
    char name[64];
    out[0] = '\0';
    if (!s->event_name || s->event_count <= 0) {
        return;
    }
    acc_game_field_to_utf8(s->event_name, s->event_name_len, name, sizeof(name));
    acc_text_tidy(name);
    if (name[0] == '\0') {
        return;
    }
    if (s->event_count > 1) {
        snprintf(out, out_size, "Event: %s, %d of %d.", name, s->event_index + 1, s->event_count);
    } else {
        snprintf(out, out_size, "Event: %s.", name);
    }
}

static void month_text(const AccCalendarState* s, char* out, int out_size) {
    snprintf(out, out_size, "%s %d%s", month_name(s->month), s->year, s->is_this_month ? ", this month" : "");
}

static void day_text(const AccCalendarState* s, char* out, int out_size) {
    static const char* const icons[3] = { "", " You played.", " You took part in an event." };
    char ev[96];
    const char* weekday = (s->weekday >= 0 && s->weekday < 7) ? s_weekday_names[s->weekday] : "";
    event_text(s, ev, sizeof(ev));
    snprintf(out, out_size, "%s %d%s%s.%s%s%s", weekday, s->day, ordinal_suffix(s->day),
             s->is_today ? ", today" : "", (s->icon >= 1 && s->icon <= 2) ? icons[s->icon] : "",
             ev[0] ? " " : "", ev);
}

void acc_calendar_opened(const u8* owner_name, int owner_len) {
    s_owner[0] = '\0';
    if (owner_name && owner_len > 0) {
        acc_game_field_to_utf8(owner_name, owner_len, s_owner, sizeof(s_owner));
    }
    s_active = FALSE; /* first update speaks the title */
    s_returning = FALSE;
}

void acc_calendar_update(const AccCalendarState* state) {
    char text[256];
    char part[192];

    if (!state) {
        return;
    }

    if (s_returning) {
        if (state->choosing_day) {
            day_text(state, text, sizeof(text));
        } else {
            month_text(state, text, sizeof(text));
        }
        acc_speak_interrupt(text);
        s_returning = FALSE;
    } else if (!s_active) {
        month_text(state, part, sizeof(part));
        if (s_owner[0]) {
            snprintf(text, sizeof(text), "%s's calendar. %s.", s_owner, part);
        } else {
            snprintf(text, sizeof(text), "Calendar. %s.", part);
        }
        acc_speak_interrupt(text);
        s_active = TRUE;
    } else if (state->choosing_day != s_last.choosing_day) {
        if (state->choosing_day) {
            day_text(state, text, sizeof(text));
        } else {
            month_text(state, part, sizeof(part));
            snprintf(text, sizeof(text), "Months. %s.", part);
        }
        acc_speak_interrupt(text);
    } else if (state->year != s_last.year || state->month != s_last.month) {
        month_text(state, text, sizeof(text));
        acc_speak_interrupt(text);
    } else if (state->choosing_day && state->day != s_last.day) {
        day_text(state, text, sizeof(text));
        acc_speak_interrupt(text);
    } else if (state->choosing_day && state->event_index != s_last.event_index) {
        event_text(state, text, sizeof(text));
        if (text[0]) {
            acc_speak_interrupt(text);
        }
    }

    s_last = *state;
    s_last.event_name = NULL; /* not kept: points into the overlay */
}

void acc_diary_opened(int month, const u8* text, int text_len, int editable) {
    char body[2048];
    char say[2200];

    body[0] = '\0';
    if (text && text_len > 0) {
        acc_game_text_to_utf8(text, text_len, body, sizeof(body));
        acc_text_tidy(body);
    }
    snprintf(say, sizeof(say), "Diary, %s. %s%s", month_name(month), body[0] ? body : "Blank page.",
             editable ? " A to write, B to close. Scroll to read line by line." : " Scroll to read line by line.");
    acc_speak_interrupt(say);
    s_returning = s_active; /* only when opened from the calendar */
}

void acc_diary_lines(int first, int last, int count, const u8* text, int text_len) {
    char body[2048];
    char say[2200];
    char where[48];

    body[0] = '\0';
    if (text && text_len > 0) {
        acc_game_text_to_utf8(text, text_len, body, sizeof(body));
        acc_text_tidy(body);
    }
    if (count <= 0) {
        count = 1;
    }
    if (last > first) {
        snprintf(where, sizeof(where), "Lines %d to %d of %d", first + 1, last + 1, count);
    } else {
        snprintf(where, sizeof(where), "Line %d of %d", first + 1, count);
    }
    if (body[0]) {
        snprintf(say, sizeof(say), "%s: %s", where, body);
    } else {
        snprintf(say, sizeof(say), "%s, blank", where);
    }
    acc_speak_interrupt(say);
}
