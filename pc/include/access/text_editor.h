/* text_editor.h - Speech for the game's text editor (letters, names,
 * bulletin board posts, diary, passwords).
 *
 * The editor can be used two ways and both are spoken:
 *  - typing on the PC keyboard (the port's game_editor input), and
 *  - the on-screen character grid moved with the stick or D-pad. */
#ifndef ACCESS_TEXT_EDITOR_H
#define ACCESS_TEXT_EDITOR_H

#include "m_editor_ovl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The editor opened. type is an mED_TYPE_* value; grid_char is the game
 * character under the on-screen grid cursor. */
void acc_text_editor_opened(const mED_Ovl_c* editor, int type, u8 grid_char);

/* Use this prompt (game-encoded) instead of the generic name the next time
 * the editor opens. Set by windows that show their own prompt, such as
 * "Enter your name.". */
void acc_text_editor_set_title(const u8* title, int title_len);

/* Name of the part of the window being edited ("Town name", "Message"),
 * or NULL for single-field editors. The game side sets this before
 * acc_text_editor_opened and before every acc_text_editor_update; moving to
 * another field is announced with that field's name and text. */
void acc_text_editor_set_field(const char* label);

/* Called once per frame while the editor is in use, after the game handled
 * the frame's input. */
void acc_text_editor_update(const mED_Ovl_c* editor, u8 grid_char);

/* The editor closed. */
void acc_text_editor_closed(void);

/* TRUE between opened and closed. */
int acc_text_editor_is_open(void);

/* Speak the whole text entered so far (read-back hotkey). */
void acc_text_editor_read_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_TEXT_EDITOR_H */
