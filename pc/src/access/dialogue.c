/* dialogue.c - Message-window speech. See access/dialogue.h.
 *
 * Message text is a byte string in the game encoding with embedded control
 * codes (CHAR_CONTROL_CODE, type byte, parameters). While a page is
 * displayed, the game's cursor code (src/game/m_msg_cursol.c_inc) replaces
 * each "put string" code in place with its text, using the mMsg_Copy*
 * functions in src/game/m_msg_main.c_inc. This module runs the same mMsg_Copy*
 * functions on a private copy of the buffer, so the live message and the
 * game state are never modified.
 *
 * Differences from the previous project's reader (2026-11-06):
 *  - Villager catchphrases, island names, letter text, "determination"
 *    strings, the capital-letter and cut-article codes are now handled; they
 *    were silently skipped before.
 *  - a.m./p.m. comes from the message's own flag, as the game displays it,
 *    not from the current clock hour.
 *  - The random-number insertion is no longer guessed; such pages wait for
 *    the game's display (see acc_dialogue_page_shown). */

#include "access/dialogue.h"

#include "access/game_text.h"
#include "access/speech.h"

#include "m_font.h"
#include "m_item_name.h"
#include "m_msg.h"
#include "m_npc.h"

#include <stdio.h>
#include <string.h>

#define ACC_DIALOGUE_TEXT_SIZE 2048

/* Last spoken page, for repeat. */
static char s_last_page[ACC_DIALOGUE_TEXT_SIZE];

/* A page that contains text we cannot know ahead of display. */
static mMsg_Window_c* s_deferred_msg = NULL;
static int s_deferred_start = -1;
static int s_deferred_named = FALSE;

typedef enum {
    PAGE_SPOKEN,   /* text produced (possibly empty) */
    PAGE_DEFERRED, /* contains a value only the game's display can supply */
} PageResult;

static void append_speaker(mMsg_Window_c* msg_p, char* out, int out_size, int* pos) {
    if (!msg_p->show_actor_name || msg_p->client_actor_p == NULL) {
        return;
    }
    u8 name[ANIMAL_NAME_LEN];
    char utf8[64];
    mNpc_GetNpcWorldName(name, msg_p->client_actor_p);
    acc_game_field_to_utf8(name, mMsg_Get_Length_String(name, ANIMAL_NAME_LEN), utf8, sizeof(utf8));
    if (utf8[0] != '\0') {
        acc_text_append(out, out_size, pos, utf8);
        acc_text_append(out, out_size, pos, ": ");
    }
}

/* Expand one "put string" code at data[i] in place using the game's own
 * function. Returns the new message length, or -1 if the code is not a
 * put-string code. *capitalize and *cut_article mirror the game's
 * mMsg_STATUS_FLAG_CAPITALIZE / _CUT_ARTICLE handling for that code. */
static int expand_put_string(mMsg_Window_c* msg_p, u8* data, int i, int len, u8 type, u32* capitalize,
                             int* cut_article) {
    const u32 cap = *capitalize;
    int article = *cut_article ? mIN_ARTICLE_NONE : mIN_ARTICLE_NUM;

    if (type >= mFont_CONT_CODE_PUT_STRING_FREE0 && type <= mFont_CONT_CODE_PUT_STRING_FREE9) {
        *capitalize = 0;
        *cut_article = FALSE;
        return mMsg_CopyFree(msg_p, mMsg_FREE_STR0 + (type - mFont_CONT_CODE_PUT_STRING_FREE0), data, i, len,
                             article, cap);
    }
    if (type >= mFont_CONT_CODE_PUT_STRING_FREE10 && type <= mFont_CONT_CODE_PUT_STRING_FREE19) {
        *capitalize = 0;
        *cut_article = FALSE;
        return mMsg_CopyFree(msg_p, mMsg_FREE_STR10 + (type - mFont_CONT_CODE_PUT_STRING_FREE10), data, i, len,
                             article, cap);
    }
    if (type >= mFont_CONT_CODE_PUT_STRING_ITEM0 && type <= mFont_CONT_CODE_PUT_STRING_ITEM4) {
        *capitalize = 0;
        *cut_article = FALSE;
        return mMsg_CopyItem(msg_p, mMsg_ITEM_STR0 + (type - mFont_CONT_CODE_PUT_STRING_ITEM0), data, i, len,
                             article, cap);
    }

    switch (type) {
        case mFont_CONT_CODE_PUT_STRING_PLAYER_NAME:
            *capitalize = 0;
            return mMsg_CopyPlayerName(data, i, len, cap);
        case mFont_CONT_CODE_PUT_STRING_TALK_NAME:
            *capitalize = 0;
            return mMsg_CopyTalkName(msg_p->client_actor_p, data, i, len, cap);
        case mFont_CONT_CODE_PUT_STRING_TAIL:
            *capitalize = 0;
            return mMsg_CopyTail(msg_p->client_actor_p, data, i, len, cap);
        case mFont_CONT_CODE_PUT_STRING_YEAR:
            return mMsg_CopyYear(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_MONTH:
            return mMsg_CopyMonth(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_WEEK:
            return mMsg_CopyWeek(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_DAY:
            return mMsg_CopyDay(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_HOUR:
            return mMsg_CopyHour(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_MIN:
            return mMsg_CopyMin(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_SEC:
            return mMsg_CopySec(data, i, len);
        case mFont_CONT_CODE_PUT_STRING_AM_PM:
            return mMsg_CopyAmPm(msg_p, data, i, len);
        case mFont_CONT_CODE_PUT_STRING_DETERMINATION:
            return mMsg_CopyDetermination(msg_p, data, i, len);
        case mFont_CONT_CODE_PUT_STRING_COUNTRY_NAME:
            *capitalize = 0;
            return mMsg_CopyCountryName(data, i, len, cap);
        case mFont_CONT_CODE_PUT_STRING_ISLAND_NAME:
            *capitalize = 0;
            return mMsg_CopyIslandName(data, i, len, cap);
        case mFont_CONT_CODE_PUT_STRING_MAIL:
            *capitalize = 0;
            return mMsg_CopyMail(msg_p, mMsg_MAIL_STR0, data, i, len, cap);
        default:
            return -1;
    }
}

/* Build the spoken text of the page starting at start_idx. */
static PageResult build_page(mMsg_Window_c* msg_p, int start_idx, int named, char* out, int out_size) {
    static u8 data[mMsg_MSG_BUF_SIZE];
    int pos = 0;
    int len = msg_p->msg_data->msg_len;
    u32 capitalize = msg_p->status_flags & mMsg_STATUS_FLAG_CAPITALIZE;
    int cut_article = (msg_p->status_flags & mMsg_STATUS_FLAG_CUT_ARTICLE) != 0;

    out[0] = '\0';
    if (len <= 0 || len > mMsg_MSG_BUF_MAX || start_idx < 0 || start_idx >= len) {
        return PAGE_SPOKEN;
    }
    memcpy(data, msg_p->msg_data->text_buf.data, (size_t)len);

    if (named) {
        append_speaker(msg_p, out, out_size, &pos);
    }

    int i = start_idx;
    while (i < len) {
        u8 c = data[i];
        if (c == CHAR_CONTROL_CODE) {
            if (i + 1 >= len) {
                break;
            }
            u8 type = data[i + 1];
            switch (type) {
                case mFont_CONT_CODE_LAST:
                case mFont_CONT_CODE_MSG_TIME_END:
                case mFont_CONT_CODE_CONTINUE:
                case mFont_CONT_CODE_BUTTON:
                case mFont_CONT_CODE_BUTTON2:
                    goto page_end;
                case mFont_CONT_CODE_PUT_STRING_RANDOM_NUMBER_2:
                    return PAGE_DEFERRED;
                case mFont_CONT_CODE_CAPITAL_LETTER:
                    capitalize = mMsg_STATUS_FLAG_CAPITALIZE;
                    break;
                case mFont_CONT_CODE_CUT_ARTICLE:
                    cut_article = TRUE;
                    break;
                case mFont_CONT_CODE_CLEAR:
                case mFont_CONT_CODE_SPACE:
                    acc_text_append(out, out_size, &pos, " ");
                    break;
                default: {
                    int new_len = expand_put_string(msg_p, data, i, len, type, &capitalize, &cut_article);
                    if (new_len >= 0) {
                        /* The inserted text now starts at i; read it as text. */
                        len = new_len;
                        continue;
                    }
                    break;
                }
            }
            i += mFont_CodeSize_idx_get(data, i);
        } else if (c == CHAR_MESSAGE_TAG) {
            i += mFont_CodeSize_idx_get(data, i);
        } else {
            const char* mapped = acc_game_char_utf8(c);
            if (mapped) {
                acc_text_append(out, out_size, &pos, mapped);
            }
            i++;
        }
    }

page_end:
    acc_text_tidy(out);
    return PAGE_SPOKEN;
}

static void speak_page(mMsg_Window_c* msg_p, int start_idx, int named) {
    char text[ACC_DIALOGUE_TEXT_SIZE];

    s_deferred_msg = NULL;
    if (!msg_p || !msg_p->msg_data || !msg_p->msg_data->data_loaded) {
        return;
    }
    if (build_page(msg_p, start_idx, named, text, sizeof(text)) == PAGE_DEFERRED) {
        s_deferred_msg = msg_p;
        s_deferred_start = start_idx;
        s_deferred_named = named;
        return;
    }
    if (text[0] != '\0') {
        snprintf(s_last_page, sizeof(s_last_page), "%s", text);
        s_last_page[sizeof(s_last_page) - 1] = '\0';
        acc_speak_interrupt(text);
    }
}

void acc_dialogue_message_loaded(mMsg_Window_c* msg_p, int new_window) {
    speak_page(msg_p, 0, new_window);
}

void acc_dialogue_page_advanced(mMsg_Window_c* msg_p, int start_idx) {
    /* When the player presses A, the game requests the cursor while
     * end_text_cursor_idx still points at the button prompt code itself; the
     * cursor steps past it on its next run (mMsg_Main_Cursol_Button_ControlCursol).
     * Skip the prompt so the next page is read, not cut off at its start.
     * (The previous project passed this index straight through.) */
    if (msg_p && msg_p->msg_data) {
        u8* data = msg_p->msg_data->text_buf.data;
        int len = msg_p->msg_data->msg_len;
        if (start_idx >= 0 && start_idx + 1 < len && data[start_idx] == CHAR_CONTROL_CODE &&
            (data[start_idx + 1] == mFont_CONT_CODE_BUTTON || data[start_idx + 1] == mFont_CONT_CODE_BUTTON2)) {
            start_idx += mFont_CodeSize_idx_get(data, start_idx);
        }
    }
    speak_page(msg_p, start_idx, FALSE);
}

void acc_dialogue_page_shown(mMsg_Window_c* msg_p, int end_idx) {
    if (s_deferred_msg == NULL || s_deferred_msg != msg_p || !msg_p->msg_data) {
        return;
    }
    /* By now the game has expanded every code on this page in the live
     * buffer, including the random number, so read it directly. */
    char text[ACC_DIALOGUE_TEXT_SIZE];
    int pos = 0;
    const u8* data = msg_p->msg_data->text_buf.data;
    int len = msg_p->msg_data->msg_len;
    if (end_idx > len) {
        end_idx = len;
    }

    text[0] = '\0';
    if (s_deferred_named) {
        append_speaker(msg_p, text, sizeof(text), &pos);
    }
    for (int i = s_deferred_start; i >= 0 && i < end_idx;) {
        u8 c = data[i];
        if (c == CHAR_CONTROL_CODE || c == CHAR_MESSAGE_TAG) {
            i += mFont_CodeSize_idx_get((u8*)data, i);
            continue;
        }
        const char* mapped = acc_game_char_utf8(c);
        if (mapped) {
            acc_text_append(text, sizeof(text), &pos, mapped);
        }
        i++;
    }
    s_deferred_msg = NULL;
    acc_text_tidy(text);
    if (text[0] != '\0') {
        snprintf(s_last_page, sizeof(s_last_page), "%s", text);
        s_last_page[sizeof(s_last_page) - 1] = '\0';
        acc_speak_interrupt(text);
    }
}

void acc_dialogue_repeat(void) {
    if (s_last_page[0] != '\0') {
        acc_speak_interrupt(s_last_page);
    }
}
