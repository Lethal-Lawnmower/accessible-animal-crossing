/* game_text.c - Game text encoding to UTF-8. See access/game_text.h.
 *
 * The table follows the CHAR_* definitions in include/m_font.h (USA font).
 * Corrections against the previous project's table (2026-11-06):
 *  - 59 is SYMBOL_DROPLET (sweat drop), not a semicolon; 208 is the semicolon.
 *  - 92 is SYMBOL_ANNOYED, not a backslash; 222 is the backslash.
 *  - 213..222 (curly quotes and apostrophes, the OE ligature, French ordinal
 *    suffixes, backslash) were missing, so those characters were silently
 *    dropped from speech.
 * 223..255 have no defined character and decode to nothing. */

#include "access/game_text.h"

#include "m_font.h"

#include <string.h>

static const char* const s_game_chars[256] = {
    [CHAR_INVERT_EXCLAMATION] = "\xC2\xA1",
    [CHAR_INVERT_QUESTIONMARK] = "\xC2\xBF",
    [CHAR_DIAERESIS_A] = "\xC3\x84",
    [CHAR_GRAVE_A] = "\xC3\x80",
    [CHAR_ACUTE_A] = "\xC3\x81",
    [CHAR_CIRCUMFLEX_A] = "\xC3\x82",
    [CHAR_TILDE_A] = "\xC3\x83",
    [CHAR_ANGSTROM_A] = "\xC3\x85",
    [CHAR_CEDILLA] = "\xC3\x87",
    [CHAR_GRAVE_E] = "\xC3\x88",
    [CHAR_ACUTE_E] = "\xC3\x89",
    [CHAR_CIRCUMFLEX_E] = "\xC3\x8A",
    [CHAR_DIARESIS_E] = "\xC3\x8B",
    [CHAR_GRAVE_I] = "\xC3\x8C",
    [CHAR_ACUTE_I] = "\xC3\x8D",
    [CHAR_CIRCUMFLEX_I] = "\xC3\x8E",
    [CHAR_DIARESIS_I] = "\xC3\x8F",
    [CHAR_ETH] = "\xC3\x90",
    [CHAR_TILDE_N] = "\xC3\x91",
    [CHAR_GRAVE_O] = "\xC3\x92",
    [CHAR_ACUTE_O] = "\xC3\x93",
    [CHAR_CIRCUMFLEX_O] = "\xC3\x94",
    [CHAR_TILDE_O] = "\xC3\x95",
    [CHAR_DIARESIS_O] = "\xC3\x96",
    [CHAR_OE] = "\xC5\x92",
    [CHAR_GRAVE_U] = "\xC3\x99",
    [CHAR_ACUTE_U] = "\xC3\x9A",
    [CHAR_CIRCUMFLEX_U] = "\xC3\x9B",
    [CHAR_DIARESIS_U] = "\xC3\x9C",
    [CHAR_LOWER_BETA] = "\xC3\x9F",
    [CHAR_THORN] = "\xC3\x9E",
    [CHAR_GRAVE_a] = "\xC3\xA0",
    [CHAR_SPACE] = " ",
    [CHAR_EXCLAMATION] = "!",
    [CHAR_QUOTATION] = "\"",
    [CHAR_ACUTE_a] = "\xC3\xA1",
    [CHAR_CIRCUMFLEX_a] = "\xC3\xA2",
    [CHAR_PERCENT] = "%",
    [CHAR_AMPERSAND] = "&",
    [CHAR_APOSTROPHE] = "'",
    [CHAR_OPEN_PARENTHESIS] = "(",
    [CHAR_CLOSE_PARENTHESIS] = ")",
    [CHAR_TILDE] = "~",
    [CHAR_SYMBOL_HEART] = " heart ",
    [CHAR_COMMA] = ",",
    [CHAR_DASH] = "-",
    [CHAR_PERIOD] = ".",
    [CHAR_SYMBOL_MUSIC_NOTE] = " music note ",
    [CHAR_ZERO] = "0",
    [CHAR_ONE] = "1",
    [CHAR_TWO] = "2",
    [CHAR_THREE] = "3",
    [CHAR_FOUR] = "4",
    [CHAR_FIVE] = "5",
    [CHAR_SIX] = "6",
    [CHAR_SEVEN] = "7",
    [CHAR_EIGHT] = "8",
    [CHAR_NINE] = "9",
    [CHAR_COLON] = ":",
    [CHAR_SYMBOL_DROPLET] = " sweat drop ",
    [CHAR_LESS_THAN] = "<",
    [CHAR_EQUALS] = "=",
    [CHAR_GREATER_THAN] = ">",
    [CHAR_QUESTIONMARK] = "?",
    [CHAR_AT_SIGN] = "@",
    [CHAR_A] = "A", [CHAR_B] = "B", [CHAR_C] = "C", [CHAR_D] = "D", [CHAR_E] = "E",
    [CHAR_F] = "F", [CHAR_G] = "G", [CHAR_H] = "H", [CHAR_I] = "I", [CHAR_J] = "J",
    [CHAR_K] = "K", [CHAR_L] = "L", [CHAR_M] = "M", [CHAR_N] = "N", [CHAR_O] = "O",
    [CHAR_P] = "P", [CHAR_Q] = "Q", [CHAR_R] = "R", [CHAR_S] = "S", [CHAR_T] = "T",
    [CHAR_U] = "U", [CHAR_V] = "V", [CHAR_W] = "W", [CHAR_X] = "X", [CHAR_Y] = "Y",
    [CHAR_Z] = "Z",
    [CHAR_TILDE_a] = "\xC3\xA3",
    [CHAR_SYMBOL_ANNOYED] = " annoyed ",
    [CHAR_DIARESIS_a] = "\xC3\xA4",
    [CHAR_ANGSTROM_a] = "\xC3\xA5",
    [CHAR_UNDERSCORE] = "_",
    [CHAR_LOWER_CEDILLA] = "\xC3\xA7",
    [CHAR_a] = "a", [CHAR_b] = "b", [CHAR_c] = "c", [CHAR_d] = "d", [CHAR_e] = "e",
    [CHAR_f] = "f", [CHAR_g] = "g", [CHAR_h] = "h", [CHAR_i] = "i", [CHAR_j] = "j",
    [CHAR_k] = "k", [CHAR_l] = "l", [CHAR_m] = "m", [CHAR_n] = "n", [CHAR_o] = "o",
    [CHAR_p] = "p", [CHAR_q] = "q", [CHAR_r] = "r", [CHAR_s] = "s", [CHAR_t] = "t",
    [CHAR_u] = "u", [CHAR_v] = "v", [CHAR_w] = "w", [CHAR_x] = "x", [CHAR_y] = "y",
    [CHAR_z] = "z",
    [CHAR_GRAVE_e] = "\xC3\xA8",
    [CHAR_ACUTE_e] = "\xC3\xA9",
    [CHAR_CIRCUMFLEX_e] = "\xC3\xAA",
    [CHAR_DIARESIS_e] = "\xC3\xAB",
    /* CHAR_CONTROL_CODE (127) and CHAR_MESSAGE_TAG (128) are not text. */
    [CHAR_GRAVE_i] = "\xC3\xAC",
    [CHAR_ACUTE_i] = "\xC3\xAD",
    [CHAR_CIRCUMFLEX_i] = "\xC3\xAE",
    [CHAR_DIARESIS_i] = "\xC3\xAF",
    [CHAR_INTERPUNCT] = "\xC2\xB7",
    [CHAR_LOWER_ETH] = "\xC3\xB0",
    [CHAR_TILDE_n] = "\xC3\xB1",
    [CHAR_GRAVE_o] = "\xC3\xB2",
    [CHAR_ACUTE_o] = "\xC3\xB3",
    [CHAR_CIRCUMFLEX_o] = "\xC3\xB4",
    [CHAR_TILDE_o] = "\xC3\xB5",
    [CHAR_DIARESIS_o] = "\xC3\xB6",
    [CHAR_oe] = "\xC5\x93",
    [CHAR_GRAVE_u] = "\xC3\xB9",
    [CHAR_ACUTE_u] = "\xC3\xBA",
    [CHAR_HYPHEN] = "-",
    [CHAR_CIRCUMFLEX_u] = "\xC3\xBB",
    [CHAR_DIARESIS_u] = "\xC3\xBC",
    [CHAR_ACUTE_y] = "\xC3\xBD",
    [CHAR_DIARESIS_y] = "\xC3\xBF",
    [CHAR_LOWER_THORN] = "\xC3\xBE",
    [CHAR_ACUTE_Y] = "\xC3\x9D",
    [CHAR_BROKEN_BAR] = "\xC2\xA6",
    [CHAR_SILCROW] = "\xC2\xA7",
    [CHAR_FEMININE_ORDINAL] = "\xC2\xAA",
    [CHAR_MASCULINE_ORDINAL] = "\xC2\xBA",
    [CHAR_DOUBLE_VERTICAL_BAR] = "\xE2\x80\x96",
    [CHAR_LATIN_MU] = "\xC2\xB5",
    [CHAR_SUPERSCRIPT_THREE] = "\xC2\xB3",
    [CHAR_SUPERSCRIPT_TWO] = "\xC2\xB2",
    [CHAR_SUPRESCRIPT_ONE] = "\xC2\xB9",
    [CHAR_MACRON_SYMBOL] = "\xC2\xAF",
    [CHAR_LOGICAL_NEGATION] = "\xC2\xAC",
    [CHAR_ASH] = "\xC3\x86",
    [CHAR_LOWER_ASH] = "\xC3\xA6",
    [CHAR_INVERT_QUOTATION] = "\xE2\x80\x9E",
    [CHAR_GUILLEMET_OPEN] = "\xC2\xAB",
    [CHAR_GUILLEMET_CLOSE] = "\xC2\xBB",
    [CHAR_SYMBOL_SUN] = " sun ",
    [CHAR_SYMBOL_CLOUD] = " cloud ",
    [CHAR_SYMBOL_UMBRELLA] = " umbrella ",
    [CHAR_SYMBOL_WIND] = " wind ",
    [CHAR_SYMBOL_SNOWMAN] = " snowman ",
    [CHAR_LINES_CONVERGE_RIGHT] = " arrow right ",
    [CHAR_LINES_CONVERGE_LEFT] = " arrow left ",
    [CHAR_FORWARD_SLASH] = "/",
    [CHAR_INFINITY] = " infinity ",
    [CHAR_CIRCLE] = " circle ",
    [CHAR_CROSS] = " cross ",
    [CHAR_SQUARE] = " square ",
    [CHAR_TRIANGLE] = " triangle ",
    [CHAR_PLUS] = "+",
    [CHAR_SYMBOL_LIGTNING] = " lightning ",
    [CHAR_MARS_SYMBOL] = " male sign ",
    [CHAR_VENUS_SYMBOL] = " female sign ",
    [CHAR_SYMBOL_FLOWER] = " flower ",
    [CHAR_SYMBOL_STAR] = " star ",
    [CHAR_SYMBOL_SKULL] = " skull ",
    [CHAR_SYMBOL_SURPRISE] = " surprised face ",
    [CHAR_SYMBOL_HAPPY] = " happy face ",
    [CHAR_SYMBOL_SAD] = " sad face ",
    [CHAR_SYMBOL_ANGRY] = " angry face ",
    [CHAR_SYMBOL_SMILE] = " smiling face ",
    [CHAR_DIMENSION_SIGN] = "\xC3\x97",
    [CHAR_OBELUS_SIGN] = "\xC3\xB7",
    [CHAR_SYMBOL_HAMMER] = " hammer ",
    [CHAR_SYMBOL_RIBBON] = " ribbon ",
    [CHAR_SYMBOL_MAIL] = " letter ",
    [CHAR_SYMBOL_MONEY] = " bells ",
    [CHAR_SYMBOL_PAW] = " paw print ",
    [CHAR_SYMBOL_SQUIRREL] = " squirrel ",
    [CHAR_SYMBOL_CAT] = " cat ",
    [CHAR_SYMBOL_RABBIT] = " rabbit ",
    [CHAR_SYMBOL_OCTOPUS] = " octopus ",
    [CHAR_SYMBOL_COW] = " cow ",
    [CHAR_SYMBOL_PIG] = " pig ",
    [CHAR_NEW_LINE] = " ",
    [CHAR_SYMBOL_FISH] = " fish ",
    [CHAR_SYMBOL_BUG] = " bug ",
    [CHAR_SEMICOLON] = ";",
    [CHAR_HASHTAG] = "#",
    [CHAR_SPACE_2] = " ",
    [CHAR_SPACE_3] = " ",
    [CHAR_SYMBOL_KEY] = " key ",
    [CHAR_LEFT_QUOTATION] = "\xE2\x80\x9C",
    [CHAR_RIGHT_QUOTATION] = "\xE2\x80\x9D",
    [CHAR_LEFT_APOSTROPHE] = "\xE2\x80\x98",
    [CHAR_RIGHT_APOSTROPHE] = "\xE2\x80\x99",
    [CHAR_ETHEL] = "\xC5\x92",
    [CHAR_LOWER_ETHEL] = "\xC5\x93",
    [CHAR_ORDINAL_e] = "e",
    [CHAR_ORDINAL_er] = "er",
    [CHAR_ORDINAL_re] = "re",
    [CHAR_BACKSLASH] = "\\",
};

const char* acc_game_char_utf8(u8 c) {
    return s_game_chars[c];
}

void acc_text_append(char* dst, int dst_size, int* pos, const char* text) {
    if (!dst || dst_size <= 0 || !pos || !text) {
        return;
    }
    int len = (int)strlen(text);
    if (*pos + len > dst_size - 1) {
        len = dst_size - 1 - *pos;
    }
    if (len > 0) {
        memcpy(dst + *pos, text, (size_t)len);
        *pos += len;
    }
    dst[*pos] = '\0';
}

int acc_game_text_to_utf8(const u8* src, int src_len, char* dst, int dst_size) {
    int pos = 0;
    if (!dst || dst_size <= 0) {
        return 0;
    }
    dst[0] = '\0';
    if (!src) {
        return 0;
    }
    for (int i = 0; i < src_len;) {
        if (src[i] == CHAR_CONTROL_CODE || src[i] == CHAR_MESSAGE_TAG) {
            /* Skip embedded codes (colours etc.) with their parameters. */
            int size = (i + 1 < src_len) ? mFont_CodeSize_idx_get((u8*)src, i) : 1;
            i += size > 0 ? size : 1;
            continue;
        }
        const char* mapped = s_game_chars[src[i]];
        if (mapped) {
            acc_text_append(dst, dst_size, &pos, mapped);
        }
        i++;
    }
    return pos;
}

int acc_game_field_to_utf8(const u8* src, int field_len, char* dst, int dst_size) {
    int len = field_len;
    while (src && len > 0 && (src[len - 1] == CHAR_SPACE || src[len - 1] == 0)) {
        len--;
    }
    int written = acc_game_text_to_utf8(src, len, dst, dst_size);
    acc_text_tidy(dst);
    (void)written;
    return (int)strlen(dst);
}

void acc_text_tidy(char* text) {
    if (!text) {
        return;
    }
    char* out = text;
    const char* in = text;
    while (*in == ' ') {
        in++;
    }
    while (*in) {
        if (*in == ' ' && (in[1] == ' ' || in[1] == '\0')) {
            in++;
            continue;
        }
        /* Drop a space that ended up before closing punctuation. */
        if (*in == ' ' && (in[1] == ',' || in[1] == '.' || in[1] == '!' || in[1] == '?')) {
            in++;
            continue;
        }
        *out++ = *in++;
    }
    *out = '\0';
}
