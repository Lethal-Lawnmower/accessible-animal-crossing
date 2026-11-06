/* mail_screens.h - Speech for letter reading, the address book and the town
 * bulletin board. */
#ifndef ACCESS_MAIL_SCREENS_H
#define ACCESS_MAIL_SCREENS_H

#include "lb_rtc.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A letter is shown for reading: header ("Dear ..."), body, footer
 * (signature). Lengths are the field lengths the game keeps. */
void acc_letter_read(const u8* header, int header_len, const u8* body, int body_len, const u8* footer,
                     int footer_len);

/* Address book, called every frame. selecting is FALSE while the book is not
 * waiting for a choice; page is mAD_PAGE_*, entry the highlighted name. */
void acc_address_update(int selecting, int page, int entry, int count, const u8* name, int name_len);

/* Bulletin board: a post is shown. opened is TRUE for the first post shown
 * after the board opens. message may be NULL when there are no posts. */
void acc_notice_post(int opened, int index, int count, const u8* message, int message_len,
                     const lbRTC_time_c* posted);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_MAIL_SCREENS_H */
