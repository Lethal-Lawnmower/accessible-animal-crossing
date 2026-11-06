/* game_text.h - Convert the game's text encoding to readable UTF-8 for speech.
 *
 * Animal Crossing stores text as one byte per character in its own encoding
 * (see the CHAR_* list in include/m_font.h). Most of 33..122 matches ASCII;
 * the rest are accented letters, typographic quotes and picture symbols.
 * Picture symbols become short spoken words ("heart", "music note").
 *
 * This decodes plain text only. Message control codes (0x7F ...) are handled
 * by the dialogue reader, which knows the message context. */
#ifndef ACCESS_GAME_TEXT_H
#define ACCESS_GAME_TEXT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UTF-8 (or a spoken word) for one game character, or NULL when the byte is
 * not a printable character (control code, message tag, unused slot). */
const char* acc_game_char_utf8(u8 c);

/* Decode src_len game bytes to UTF-8 in dst (always NUL-terminated).
 * Game newlines become spaces. Returns bytes written, excluding the NUL. */
int acc_game_text_to_utf8(const u8* src, int src_len, char* dst, int dst_size);

/* Like acc_game_text_to_utf8, but first trims trailing padding spaces, which
 * the game uses to fill fixed-length name and label fields. */
int acc_game_field_to_utf8(const u8* src, int field_len, char* dst, int dst_size);

/* Append a NUL-terminated UTF-8 string at *pos, keeping dst terminated.
 * Stops quietly when dst is full. */
void acc_text_append(char* dst, int dst_size, int* pos, const char* text);

/* Collapse runs of spaces and trim both ends, in place. */
void acc_text_tidy(char* text);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_GAME_TEXT_H */
