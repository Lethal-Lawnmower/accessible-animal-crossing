/* pc_acc_msg.c - Dialogue TTS hook: converts game message buffer to UTF-8,
 * sends to screen reader via pc_accessibility.h.
 *
 * Called from mMsg_MainSetup_Appear (m_msg_appear.c_inc) when a new dialogue
 * message is loaded and ready to display. We walk the raw text buffer, convert
 * the game's custom character encoding to UTF-8, resolve control codes that
 * insert text (player name, NPC name, items, etc.), and skip non-text codes
 * (color, sound, demo orders). The result is a clean readable string sent to
 * TTS all at once so the player doesn't have to wait for the slow character-
 * by-character display. */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "m_msg.h"
#include "m_font.h"
#include "m_private.h"
#include "m_common_data.h"
#include "m_npc.h"
#include "m_land_h.h"
#include "m_item_name.h"
#include "m_personal_id.h"
#include "m_string.h"
#include "lb_rtc.h"

#include <string.h>
#include <stdio.h>

/* ========================================================================= */
/* Game character encoding → UTF-8 lookup table                              */
/* ========================================================================= */
/* The game's font uses a custom byte encoding. Positions 33-122 map to ASCII
 * directly. Other positions map to accented chars, symbols, or specials.
 * We convert to UTF-8 strings. NULL = skip/unprintable. */

static const char* s_char_to_utf8[256] = {
    /* 0x00 */ "\xC2\xA1",  /* ¡ INVERT_EXCLAMATION */
    /* 0x01 */ "\xC2\xBF",  /* ¿ INVERT_QUESTIONMARK */
    /* 0x02 */ "\xC3\x84",  /* Ä DIAERESIS_A */
    /* 0x03 */ "\xC3\x80",  /* À GRAVE_A */
    /* 0x04 */ "\xC3\x81",  /* Á ACUTE_A */
    /* 0x05 */ "\xC3\x82",  /* Â CIRCUMFLEX_A */
    /* 0x06 */ "\xC3\x83",  /* Ã TILDE_A */
    /* 0x07 */ "\xC3\x85",  /* Å ANGSTROM_A */
    /* 0x08 */ "\xC3\x87",  /* Ç CEDILLA */
    /* 0x09 */ "\xC3\x88",  /* È GRAVE_E */
    /* 0x0A */ "\xC3\x89",  /* É ACUTE_E */
    /* 0x0B */ "\xC3\x8A",  /* Ê CIRCUMFLEX_E */
    /* 0x0C */ "\xC3\x8B",  /* Ë DIARESIS_E */
    /* 0x0D */ "\xC3\x8C",  /* Ì GRAVE_I */
    /* 0x0E */ "\xC3\x8D",  /* Í ACUTE_I */
    /* 0x0F */ "\xC3\x8E",  /* Î CIRCUMFLEX_I */
    /* 0x10 */ "\xC3\x8F",  /* Ï DIARESIS_I */
    /* 0x11 */ "\xC3\x90",  /* Ð ETH */
    /* 0x12 */ "\xC3\x91",  /* Ñ TILDE_N */
    /* 0x13 */ "\xC3\x92",  /* Ò GRAVE_O */
    /* 0x14 */ "\xC3\x93",  /* Ó ACUTE_O */
    /* 0x15 */ "\xC3\x94",  /* Ô CIRCUMFLEX_O */
    /* 0x16 */ "\xC3\x95",  /* Õ TILDE_O */
    /* 0x17 */ "\xC3\x96",  /* Ö DIARESIS_O */
    /* 0x18 */ "\xC5\x92",  /* Œ OE */
    /* 0x19 */ "\xC3\x99",  /* Ù GRAVE_U */
    /* 0x1A */ "\xC3\x9A",  /* Ú ACUTE_U */
    /* 0x1B */ "\xC3\x9B",  /* Û CIRCUMFLEX_U */
    /* 0x1C */ "\xC3\x9C",  /* Ü DIARESIS_U */
    /* 0x1D */ "\xC3\x9F",  /* ß LOWER_BETA */
    /* 0x1E */ "\xC3\x9E",  /* Þ THORN */
    /* 0x1F */ "\xC3\xA0",  /* à GRAVE_a */
    /* 0x20 */ " ",          /* SPACE */
    /* 0x21 */ "!",          /* EXCLAMATION */
    /* 0x22 */ "\"",         /* QUOTATION */
    /* 0x23 */ "\xC3\xA1",  /* á ACUTE_a */
    /* 0x24 */ "\xC3\xA2",  /* â CIRCUMFLEX_a */
    /* 0x25 */ "%",          /* PERCENT */
    /* 0x26 */ "&",          /* AMPERSAND */
    /* 0x27 */ "'",          /* APOSTROPHE */
    /* 0x28 */ "(",          /* OPEN_PARENTHESIS */
    /* 0x29 */ ")",          /* CLOSE_PARENTHESIS */
    /* 0x2A */ "~",          /* TILDE */
    /* 0x2B */ "heart",      /* HEART symbol */
    /* 0x2C */ ",",          /* COMMA */
    /* 0x2D */ "-",          /* DASH */
    /* 0x2E */ ".",          /* PERIOD */
    /* 0x2F */ "note",       /* MUSIC_NOTE symbol */
    /* 0x30-0x39: digits */
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    /* 0x3A */ ":",          /* COLON */
    /* 0x3B */ ";",          /* DROPLET/SEMICOLON - actually sweat drop but ; on keyboard */
    /* 0x3C */ "<",          /* LESS_THAN */
    /* 0x3D */ "=",          /* EQUALS */
    /* 0x3E */ ">",          /* GREATER_THAN */
    /* 0x3F */ "?",          /* QUESTIONMARK */
    /* 0x40 */ "@",          /* AT_SIGN */
    /* 0x41-0x5A: A-Z */
    "A","B","C","D","E","F","G","H","I","J","K","L","M",
    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    /* 0x5B */ "\xC3\xA3",  /* ã TILDE_a */
    /* 0x5C */ "\\",         /* ANNOYED/BACKSLASH symbol */
    /* 0x5D */ "\xC3\xA4",  /* ä DIARESIS_a */
    /* 0x5E */ "\xC3\xA5",  /* å ANGSTROM_a */
    /* 0x5F */ "_",          /* UNDERSCORE */
    /* 0x60 */ "\xC3\xA7",  /* ç LOWER_CEDILLA */
    /* 0x61-0x7A: a-z */
    "a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","o","p","q","r","s","t","u","v","w","x","y","z",
    /* 0x7B */ "\xC3\xA8",  /* è GRAVE_e */
    /* 0x7C */ "\xC3\xA9",  /* é ACUTE_e */
    /* 0x7D */ "\xC3\xAA",  /* ê CIRCUMFLEX_e */
    /* 0x7E */ "\xC3\xAB",  /* ë DIARESIS_e */
    /* 0x7F */ NULL,         /* CONTROL_CODE - handled separately */
    /* 0x80 */ NULL,         /* MESSAGE_TAG - handled separately */
    /* 0x81 */ "\xC3\xAC",  /* ì GRAVE_i */
    /* 0x82 */ "\xC3\xAD",  /* í ACUTE_i */
    /* 0x83 */ "\xC3\xAE",  /* î CIRCUMFLEX_i */
    /* 0x84 */ "\xC3\xAF",  /* ï DIARESIS_i */
    /* 0x85 */ "\xC2\xB7",  /* · INTERPUNCT */
    /* 0x86 */ "\xC3\xB0",  /* ð LOWER_ETH */
    /* 0x87 */ "\xC3\xB1",  /* ñ TILDE_n */
    /* 0x88 */ "\xC3\xB2",  /* ò GRAVE_o */
    /* 0x89 */ "\xC3\xB3",  /* ó ACUTE_o */
    /* 0x8A */ "\xC3\xB4",  /* ô CIRCUMFLEX_o */
    /* 0x8B */ "\xC3\xB5",  /* õ TILDE_o */
    /* 0x8C */ "\xC3\xB6",  /* ö DIARESIS_o */
    /* 0x8D */ "\xC5\x93",  /* œ oe */
    /* 0x8E */ "\xC3\xB9",  /* ù GRAVE_u */
    /* 0x8F */ "\xC3\xBA",  /* ú ACUTE_u */
    /* 0x90 */ "-",          /* HYPHEN */
    /* 0x91 */ "\xC3\xBB",  /* û CIRCUMFLEX_u */
    /* 0x92 */ "\xC3\xBC",  /* ü DIARESIS_u */
    /* 0x93 */ "\xC3\xBD",  /* ý ACUTE_y */
    /* 0x94 */ "\xC3\xBF",  /* ÿ DIARESIS_y */
    /* 0x95 */ "\xC3\xBE",  /* þ LOWER_THORN */
    /* 0x96 */ "\xC3\x9D",  /* Ý ACUTE_Y */
    /* 0x97 */ "\xC2\xA6",  /* ¦ BROKEN_BAR */
    /* 0x98 */ "\xC2\xA7",  /* § SILCROW/SECTION */
    /* 0x99 */ "\xC2\xAA",  /* ª FEMININE_ORDINAL */
    /* 0x9A */ "\xC2\xBA",  /* º MASCULINE_ORDINAL */
    /* 0x9B */ "||",         /* DOUBLE_VERTICAL_BAR */
    /* 0x9C */ "\xC2\xB5",  /* µ LATIN_MU */
    /* 0x9D */ "\xC2\xB3",  /* ³ SUPERSCRIPT_THREE */
    /* 0x9E */ "\xC2\xB2",  /* ² SUPERSCRIPT_TWO */
    /* 0x9F */ "\xC2\xB9",  /* ¹ SUPERSCRIPT_ONE */
    /* 0xA0 */ "\xC2\xAF",  /* ¯ MACRON_SYMBOL */
    /* 0xA1 */ "\xC2\xAC",  /* ¬ LOGICAL_NEGATION */
    /* 0xA2 */ "\xC3\x86",  /* Æ ASH */
    /* 0xA3 */ "\xC3\xA6",  /* æ LOWER_ASH */
    /* 0xA4 */ "\xC2\xAB",  /* « INVERT_QUOTATION */
    /* 0xA5 */ "\xC2\xAB",  /* « GUILLEMET_OPEN */
    /* 0xA6 */ "\xC2\xBB",  /* » GUILLEMET_CLOSE */
    /* 0xA7 */ "sun",        /* SUN symbol */
    /* 0xA8 */ "cloud",      /* CLOUD symbol */
    /* 0xA9 */ "umbrella",   /* UMBRELLA symbol */
    /* 0xAA */ "wind",       /* WIND symbol */
    /* 0xAB */ "snowman",    /* SNOWMAN symbol */
    /* 0xAC */ ">>",         /* LINES_CONVERGE_RIGHT */
    /* 0xAD */ "<<",         /* LINES_CONVERGE_LEFT */
    /* 0xAE */ "/",          /* FORWARD_SLASH */
    /* 0xAF */ "infinity",   /* INFINITY */
    /* 0xB0 */ "circle",     /* CIRCLE */
    /* 0xB1 */ "cross",      /* CROSS */
    /* 0xB2 */ "square",     /* SQUARE */
    /* 0xB3 */ "triangle",   /* TRIANGLE */
    /* 0xB4 */ "+",          /* PLUS */
    /* 0xB5 */ "lightning",  /* LIGHTNING */
    /* 0xB6 */ "male",       /* MARS/MALE */
    /* 0xB7 */ "female",     /* VENUS/FEMALE */
    /* 0xB8 */ "flower",     /* FLOWER */
    /* 0xB9 */ "star",       /* STAR */
    /* 0xBA */ "skull",      /* SKULL */
    /* 0xBB */ "surprise",   /* SURPRISE */
    /* 0xBC */ "happy",      /* HAPPY */
    /* 0xBD */ "sad",        /* SAD */
    /* 0xBE */ "angry",      /* ANGRY */
    /* 0xBF */ "smile",      /* SMILE */
    /* 0xC0 */ "x",          /* DIMENSION (multiply) */
    /* 0xC1 */ "\xC3\xB7",  /* ÷ OBELUS (divide) */
    /* 0xC2 */ "hammer",     /* HAMMER */
    /* 0xC3 */ "ribbon",     /* RIBBON */
    /* 0xC4 */ "mail",       /* MAIL */
    /* 0xC5 */ "bells",      /* MONEY/BELLS */
    /* 0xC6 */ "paw",        /* PAW */
    /* 0xC7 */ "squirrel",   /* SQUIRREL */
    /* 0xC8 */ "cat",        /* CAT */
    /* 0xC9 */ "rabbit",     /* RABBIT */
    /* 0xCA */ "octopus",    /* OCTOPUS */
    /* 0xCB */ "cow",        /* COW */
    /* 0xCC */ "pig",        /* PIG */
    /* 0xCD */ "\n",         /* NEW_LINE */
    /* 0xCE */ "fish",       /* FISH */
    /* 0xCF */ "bug",        /* BUG */
    /* 0xD0 */ ";",          /* SEMICOLON */
    /* 0xD1 */ "#",          /* HASHTAG */
    /* 0xD2 */ " ",          /* SPACE_2 */
    /* 0xD3 */ " ",          /* SPACE_3 */
    /* 0xD4 */ "key",        /* KEY */
    /* 0xD5-0xFF: EU-only and unused (43 entries) */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* D5-DC */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* DD-E4 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* E5-EC */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* ED-F4 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* F5-FC */
    NULL, NULL, NULL,                                /* FD-FF */
};

/* ========================================================================= */
/* Convert a game-encoded string to UTF-8                                    */
/* ========================================================================= */
int pc_acc_game_str_to_utf8(const u8* src, int src_len, char* dst, int dst_size) {
    int di = 0;
    for (int i = 0; i < src_len && di < dst_size - 1; i++) {
        u8 c = src[i];
        const char* mapped = s_char_to_utf8[c];
        if (mapped) {
            int len = (int)strlen(mapped);
            if (di + len >= dst_size - 1) break;
            memcpy(dst + di, mapped, len);
            di += len;
        }
    }
    dst[di] = '\0';
    return di;
}

/* ========================================================================= */
/* Walk the raw message buffer, decode text and resolve control codes         */
/* ========================================================================= */

/* Maximum output buffer for TTS string */
#define ACC_MSG_BUF_SIZE 2048

static void pc_acc_msg_speak_internal(mMsg_Window_c* msg_p, int start_idx) {
    if (!pc_acc_is_active()) return;
    if (!msg_p || !msg_p->msg_data || !msg_p->msg_data->data_loaded) return;

    u8* data = msg_p->msg_data->text_buf.data;
    int len = msg_p->msg_data->msg_len;
    char out[ACC_MSG_BUF_SIZE];
    int oi = 0;
    int i = start_idx;

    /* Prepend speaker name on first page of a new message */
    if (start_idx == 0 && msg_p->show_actor_name && msg_p->client_actor_p) {
        u8 name[ANIMAL_NAME_LEN];
        mNpc_GetNpcWorldName(name, msg_p->client_actor_p);
        int nlen = mMsg_Get_Length_String(name, ANIMAL_NAME_LEN);
        if (nlen > 0) {
            oi += pc_acc_game_str_to_utf8(name, nlen, out + oi, ACC_MSG_BUF_SIZE - oi);
            if (oi > 0 && oi < ACC_MSG_BUF_SIZE - 8) {
                memcpy(out + oi, " says: ", 7);
                oi += 7;
            }
        }
    }

    while (i < len && oi < ACC_MSG_BUF_SIZE - 8) {
        u8 c = data[i];

        if (c == CHAR_CONTROL_CODE) {
            /* Control code: 0x7F + type byte + optional params */
            if (i + 1 >= len) break;
            u8 type = data[i + 1];
            int code_size = mFont_CodeSize_idx_get(data, i);

            switch (type) {
                /* Text insertion control codes — resolve from msg_p fields */
                case mFont_CONT_CODE_PUT_STRING_PLAYER_NAME: {
                    u8* name = Common_Get(now_private)->player_ID.player_name;
                    int nlen = mMsg_Get_Length_String(name, PLAYER_NAME_LEN);
                    oi += pc_acc_game_str_to_utf8(name, nlen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_TALK_NAME: {
                    if (msg_p->client_actor_p) {
                        u8 name[ANIMAL_NAME_LEN];
                        mNpc_GetNpcWorldName(name, msg_p->client_actor_p);
                        int nlen = mMsg_Get_Length_String(name, ANIMAL_NAME_LEN);
                        oi += pc_acc_game_str_to_utf8(name, nlen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    }
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_COUNTRY_NAME: {
                    u8* name = Save_Get(land_info.name);
                    int nlen = mMsg_Get_Length_String(name, LAND_NAME_SIZE);
                    oi += pc_acc_game_str_to_utf8(name, nlen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                /* Free strings (pre-set by game code before dialogue) */
                case mFont_CONT_CODE_PUT_STRING_FREE0:
                case mFont_CONT_CODE_PUT_STRING_FREE1:
                case mFont_CONT_CODE_PUT_STRING_FREE2:
                case mFont_CONT_CODE_PUT_STRING_FREE3:
                case mFont_CONT_CODE_PUT_STRING_FREE4:
                case mFont_CONT_CODE_PUT_STRING_FREE5:
                case mFont_CONT_CODE_PUT_STRING_FREE6:
                case mFont_CONT_CODE_PUT_STRING_FREE7:
                case mFont_CONT_CODE_PUT_STRING_FREE8:
                case mFont_CONT_CODE_PUT_STRING_FREE9: {
                    int str_no = type - mFont_CONT_CODE_PUT_STRING_FREE0;
                    u8* str = msg_p->free_str[str_no];
                    int slen = mMsg_Get_Length_String(str, mMsg_FREE_STRING_LEN);
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_FREE10:
                case mFont_CONT_CODE_PUT_STRING_FREE11:
                case mFont_CONT_CODE_PUT_STRING_FREE12:
                case mFont_CONT_CODE_PUT_STRING_FREE13:
                case mFont_CONT_CODE_PUT_STRING_FREE14:
                case mFont_CONT_CODE_PUT_STRING_FREE15:
                case mFont_CONT_CODE_PUT_STRING_FREE16:
                case mFont_CONT_CODE_PUT_STRING_FREE17:
                case mFont_CONT_CODE_PUT_STRING_FREE18:
                case mFont_CONT_CODE_PUT_STRING_FREE19: {
                    int str_no = 10 + (type - mFont_CONT_CODE_PUT_STRING_FREE10);
                    u8* str = msg_p->free_str[str_no];
                    int slen = mMsg_Get_Length_String(str, mMsg_FREE_STRING_LEN);
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                /* Item name strings */
                case mFont_CONT_CODE_PUT_STRING_ITEM0:
                case mFont_CONT_CODE_PUT_STRING_ITEM1:
                case mFont_CONT_CODE_PUT_STRING_ITEM2:
                case mFont_CONT_CODE_PUT_STRING_ITEM3:
                case mFont_CONT_CODE_PUT_STRING_ITEM4: {
                    int str_no = type - mFont_CONT_CODE_PUT_STRING_ITEM0;
                    u8* str = msg_p->item_str[str_no];
                    int slen = mMsg_Get_Length_String(str, mIN_ITEM_NAME_LEN);
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                /* Date/time strings — resolve from RTC */
                case mFont_CONT_CODE_PUT_STRING_YEAR: {
                    u8 str[6];
                    int slen = mString_Load_YearStringFromRom(str, Common_Get(time.rtc_time.year));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_MONTH: {
                    u8 str[9];
                    int slen = mString_Load_MonthStringFromRom(str, Common_Get(time.rtc_time.month));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_WEEK: {
                    u8 str[9];
                    int slen = mString_Load_WeekStringFromRom(str, Common_Get(time.rtc_time.weekday));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_DAY: {
                    u8 str[4];
                    int slen = mString_Load_DayStringFromRom(str, Common_Get(time.rtc_time.day));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_HOUR: {
                    u8 str[15];
                    int slen = mString_Load_HourStringFromRom2(str, Common_Get(time.rtc_time.hour));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_MIN: {
                    u8 str[4];
                    int slen = mString_Load_MinStringFromRom(str, Common_Get(time.rtc_time.min));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_SEC: {
                    u8 str[5];
                    int slen = mString_Load_SecStringFromRom(str, Common_Get(time.rtc_time.sec));
                    oi += pc_acc_game_str_to_utf8(str, slen, out + oi, ACC_MSG_BUF_SIZE - oi);
                    break;
                }
                case mFont_CONT_CODE_PUT_STRING_AM_PM: {
                    u8 hour = Common_Get(time.rtc_time.hour);
                    const char* ampm = (hour < 12) ? "a.m." : "p.m.";
                    int ampm_len = (int)strlen(ampm);
                    if (oi + ampm_len < ACC_MSG_BUF_SIZE - 1) {
                        memcpy(out + oi, ampm, ampm_len);
                        oi += ampm_len;
                    }
                    break;
                }
                /* Newline / page clear — insert space for TTS flow */
                case mFont_CONT_CODE_CLEAR:
                    if (oi > 0 && out[oi - 1] != ' ') {
                        out[oi++] = ' ';
                    }
                    break;

                /* Terminal codes — stop scanning */
                case mFont_CONT_CODE_LAST:
                case mFont_CONT_CODE_MSG_TIME_END:
                    goto done;

                case mFont_CONT_CODE_CONTINUE:
                    /* Message continues in another bubble — stop here, next part
                     * will be spoken when it loads via mMsg_ChangeMsgData */
                    goto done;

                case mFont_CONT_CODE_BUTTON:
                case mFont_CONT_CODE_BUTTON2:
                    /* Button prompt — player presses A to see next page.
                     * Stop here; the next page will be spoken when the
                     * cursor advances past this code (hooked separately). */
                    goto done;

                /* Everything else (color, sound, demo, select, etc.) — skip */
                default:
                    break;
            }

            i += code_size;
        }
        else if (c == CHAR_NEW_LINE) {
            /* Newline → space for TTS */
            if (oi > 0 && out[oi - 1] != ' ') {
                out[oi++] = ' ';
            }
            i++;
        }
        else if (c == CHAR_MESSAGE_TAG) {
            /* Skip message tags */
            i += mFont_CodeSize_idx_get(data, i);
        }
        else {
            /* Regular character — convert via lookup table */
            const char* mapped = s_char_to_utf8[c];
            if (mapped) {
                int mlen = (int)strlen(mapped);
                if (oi + mlen < ACC_MSG_BUF_SIZE - 1) {
                    memcpy(out + oi, mapped, mlen);
                    oi += mlen;
                }
            }
            i++;
        }
    }

done:
    out[oi] = '\0';

    /* Trim trailing whitespace */
    while (oi > 0 && out[oi - 1] == ' ') {
        out[--oi] = '\0';
    }

    if (oi > 0) {
        printf("[ACC_MSG] TTS (%d chars, start=%d stopped at byte %d/%d): \"%s\"\n", oi, start_idx, i, len, out);
        pc_acc_speak_interrupt(out);
    } else {
        printf("[ACC_MSG] TTS empty (start=%d stopped at byte %d/%d)\n", start_idx, i, len);
    }
}

void pc_acc_msg_speak(mMsg_Window_c* msg_p) {
    pc_acc_msg_speak_internal(msg_p, 0);
}

void pc_acc_msg_speak_from(mMsg_Window_c* msg_p, int start_idx) {
    pc_acc_msg_speak_internal(msg_p, start_idx);
}

#else
/* Non-Windows stubs */
#include "m_msg.h"
void pc_acc_msg_speak(mMsg_Window_c* msg_p) { (void)msg_p; }
void pc_acc_msg_speak_from(mMsg_Window_c* msg_p, int start_idx) { (void)msg_p; (void)start_idx; }
int pc_acc_game_str_to_utf8(const u8* src, int src_len, char* dst, int dst_size) {
    (void)src; (void)src_len; (void)dst_size;
    if (dst && dst_size > 0) dst[0] = '\0';
    return 0;
}
#endif
