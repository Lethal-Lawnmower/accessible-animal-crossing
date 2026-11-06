/* calendar.h - Speech for the calendar and the diary opened from it.
 *
 * The calendar has two stable states: choosing a month (stick left/right)
 * and choosing a day inside that month (C-stick). The game side calls
 * acc_calendar_update once per frame in either state; this module speaks
 * only what changed. */
#ifndef ACCESS_CALENDAR_H
#define ACCESS_CALENDAR_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int choosing_day;  /* FALSE: choosing a month; TRUE: choosing a day */
    int year;
    int month;         /* 1..12 */
    int is_this_month;
    int day;           /* 1..31, only when choosing_day */
    int weekday;       /* 0 = Sunday */
    int is_today;
    int icon;          /* 0 none, 1 played that day, 2 took part in an event */
    const u8* event_name;  /* game-encoded, NULL when the day has no event */
    int event_name_len;
    int event_index;   /* which of event_count events is shown */
    int event_count;
} AccCalendarState;

/* The calendar opened. owner_name is game-encoded (player name), may be NULL. */
void acc_calendar_opened(const u8* owner_name, int owner_len);

void acc_calendar_update(const AccCalendarState* state);

/* A diary page opened (read view). month is 1..12; editable is TRUE when
 * A starts writing. */
void acc_diary_opened(int month, const u8* text, int text_len, int editable);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_CALENDAR_H */
