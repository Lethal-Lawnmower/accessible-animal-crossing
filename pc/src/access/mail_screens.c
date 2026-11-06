/* mail_screens.c - Letters, address book, bulletin board. See
 * access/mail_screens.h.
 *
 * Carried over from the previous project's pc_acc_menu.c, with changes:
 *  - Address book: the selection is checked once per frame instead of at
 *    each place the game moves the cursor, so mouse selection and page turns
 *    are covered too. The "Choose an addressee" prompt is no longer cut off
 *    by the name spoken right after it, and page changes are announced.
 *  - Bulletin board: the post is announced when the page has finished
 *    turning (mNT_Play_page_move), including multi-page jumps, which the old
 *    hook announced before the animation had reached the target page. */

#include "access/mail_screens.h"

#include "access/game_text.h"
#include "access/speech.h"

#include "m_address_ovl.h"

#include <stdio.h>

static const char* const s_months[13] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};

/* ---- Letters ------------------------------------------------------------- */

void acc_letter_read(const u8* header, int header_len, const u8* body, int body_len, const u8* footer,
                     int footer_len) {
    const u8* parts[3] = {header, body, footer};
    const int lens[3] = {header_len, body_len, footer_len};
    char text[1024];
    int pos = 0;

    text[0] = '\0';
    for (int i = 0; i < 3; i++) {
        char part[512];
        if (!parts[i] || lens[i] <= 0) {
            continue;
        }
        acc_game_field_to_utf8(parts[i], lens[i], part, sizeof(part));
        if (part[0] == '\0') {
            continue;
        }
        if (pos > 0) {
            acc_text_append(text, sizeof(text), &pos, " ");
        }
        acc_text_append(text, sizeof(text), &pos, part);
    }
    acc_speak_interrupt(pos > 0 ? text : "Empty letter");
}

/* ---- Address book -------------------------------------------------------- */

static int s_addr_selecting = FALSE;
static int s_addr_page = -1;
static int s_addr_entry = -1;

static const char* page_name(int page) {
    switch (page) {
        case mAD_PAGE_PLAYER: return "Town residents";
        case mAD_PAGE_NPC0:   return "Neighbors, page 1";
        case mAD_PAGE_NPC1:   return "Neighbors, page 2";
        default:              return NULL;
    }
}

void acc_address_update(int selecting, int page, int entry, int count, const u8* name, int name_len) {
    char label[48];
    char text[160];

    if (!selecting) {
        s_addr_selecting = FALSE;
        return;
    }
    if (s_addr_selecting && page == s_addr_page && entry == s_addr_entry) {
        return;
    }

    acc_game_field_to_utf8(name, name_len, label, sizeof(label));
    snprintf(text, sizeof(text), "%s, %d of %d", label[0] ? label : "No name", entry + 1, count);

    if (!s_addr_selecting) {
        /* Just started choosing: prompt and page first, the name queued. */
        const char* pname = page_name(page);
        char intro[96];
        snprintf(intro, sizeof(intro), "Choose an addressee. %s.", pname ? pname : "");
        acc_speak_interrupt(intro);
        acc_speak_queue(text);
    } else if (page != s_addr_page) {
        const char* pname = page_name(page);
        if (pname) {
            acc_speak_interrupt(pname);
            acc_speak_queue(text);
        } else {
            acc_speak_interrupt(text);
        }
    } else {
        acc_speak_interrupt(text);
    }

    s_addr_selecting = TRUE;
    s_addr_page = page;
    s_addr_entry = entry;
}

/* ---- Bulletin board ------------------------------------------------------ */

void acc_notice_post(int opened, int index, int count, const u8* message, int message_len,
                     const lbRTC_time_c* posted) {
    char body[512];
    char text[700];

    if (count <= 0 || !message) {
        acc_speak_interrupt(opened ? "Bulletin board. No posts." : "No posts");
        return;
    }
    acc_game_field_to_utf8(message, message_len, body, sizeof(body));
    if (posted && posted->month >= 1 && posted->month <= 12) {
        snprintf(text, sizeof(text), "%sPost %d of %d, %s %d, %d. %s", opened ? "Bulletin board. " : "", index + 1,
                 count, s_months[posted->month], posted->day, posted->year, body);
    } else {
        snprintf(text, sizeof(text), "%sPost %d of %d. %s", opened ? "Bulletin board. " : "", index + 1, count,
                 body);
    }
    acc_speak_interrupt(text);
}
