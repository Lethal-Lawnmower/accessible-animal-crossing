/* text_editor.c - Text editor speech. See access/text_editor.h.
 *
 * Carried over from the previous project's pc_acc_menu.c editor code, with
 * changes:
 *  - The opening announcement names what is being written and reads any
 *    existing text, instead of only "Keyboard, lowercase".
 *  - Backspace says which character was deleted.
 *  - Moving the text cursor reads the character it lands on; previously only
 *    the on-screen grid cursor was spoken.
 *  - Typed characters use the game's own encoding table (game_text.c) for
 *    accented letters and symbols, with spelled-out names for punctuation. */

#include "access/text_editor.h"

#include "access/game_text.h"
#include "access/speech.h"

#include "m_font.h"

#include <stdio.h>
#include <string.h>

#define ACC_EDITOR_TEXT_MAX 512

typedef struct {
    int active;
    u8 grid_col;
    u8 grid_row;
    u8 input_mode;
    u8 shift_mode;
    s16 cursor_idx;
    s16 length;
    u8 text[ACC_EDITOR_TEXT_MAX];
    const u8* live_text;
} EditorSnapshot;

static EditorSnapshot s_last;

/* One-shot prompt supplied by the window that opens the editor. */
static char s_pending_title[64];

void acc_text_editor_set_title(const u8* title, int title_len) {
    s_pending_title[0] = '\0';
    if (title && title_len > 0) {
        acc_game_field_to_utf8(title, title_len, s_pending_title, sizeof(s_pending_title));
    }
}

static const char* editor_name(int type) {
    switch (type) {
        case mED_TYPE_BOARD:        return "Letter";
        case mED_TYPE_HBOARD:       return "Gyroid message";
        case mED_TYPE_NOTICE:       return "Bulletin board post";
        case mED_TYPE_LEDIT:        return "Text entry";
        case mED_TYPE_CP_TITLE:     return "Folder name";
        case mED_TYPE_DIARY:        return "Diary";
        case mED_TYPE_PASSWORDMAKE: return "Password";
        case mED_TYPE_PASSWORDCHK:  return "Password";
        default:                    return "Text entry";
    }
}

static const char* mode_name(int input_mode, int shift_mode) {
    if (input_mode == mED_INPUT_MODE_SIGN) {
        return "symbols";
    }
    if (input_mode == mED_INPUT_MODE_MARK) {
        return "marks";
    }
    return shift_mode == mED_SHIFT_UPPER ? "uppercase" : "lowercase";
}

/* A character name that a speech engine will not drop or confuse. */
static void char_name(u8 c, char* out, int out_size) {
    if (c >= 'A' && c <= 'Z') {
        snprintf(out, out_size, "capital %c", (char)c);
        return;
    }
    if (c >= '0' && c <= '9') {
        snprintf(out, out_size, "%c", (char)c);
        return;
    }
    switch (c) {
        case CHAR_SPACE:
        case CHAR_SPACE_2:
        case CHAR_SPACE_3:       snprintf(out, out_size, "space"); return;
        case CHAR_NEW_LINE:      snprintf(out, out_size, "new line"); return;
        case CHAR_EXCLAMATION:   snprintf(out, out_size, "exclamation mark"); return;
        case CHAR_QUOTATION:     snprintf(out, out_size, "quote"); return;
        case CHAR_PERCENT:       snprintf(out, out_size, "percent"); return;
        case CHAR_AMPERSAND:     snprintf(out, out_size, "and sign"); return;
        case CHAR_APOSTROPHE:    snprintf(out, out_size, "apostrophe"); return;
        case CHAR_OPEN_PARENTHESIS:  snprintf(out, out_size, "left paren"); return;
        case CHAR_CLOSE_PARENTHESIS: snprintf(out, out_size, "right paren"); return;
        case CHAR_TILDE:         snprintf(out, out_size, "tilde"); return;
        case CHAR_COMMA:         snprintf(out, out_size, "comma"); return;
        case CHAR_DASH:
        case CHAR_HYPHEN:        snprintf(out, out_size, "dash"); return;
        case CHAR_PERIOD:        snprintf(out, out_size, "period"); return;
        case CHAR_COLON:         snprintf(out, out_size, "colon"); return;
        case CHAR_SEMICOLON:     snprintf(out, out_size, "semicolon"); return;
        case CHAR_LESS_THAN:     snprintf(out, out_size, "less than"); return;
        case CHAR_EQUALS:        snprintf(out, out_size, "equals"); return;
        case CHAR_GREATER_THAN:  snprintf(out, out_size, "greater than"); return;
        case CHAR_QUESTIONMARK:  snprintf(out, out_size, "question mark"); return;
        case CHAR_AT_SIGN:       snprintf(out, out_size, "at sign"); return;
        case CHAR_UNDERSCORE:    snprintf(out, out_size, "underscore"); return;
        case CHAR_FORWARD_SLASH: snprintf(out, out_size, "slash"); return;
        case CHAR_BACKSLASH:     snprintf(out, out_size, "backslash"); return;
        case CHAR_PLUS:          snprintf(out, out_size, "plus"); return;
        case CHAR_HASHTAG:       snprintf(out, out_size, "number sign"); return;
        default:
            break;
    }
    const char* mapped = acc_game_char_utf8(c);
    if (mapped) {
        snprintf(out, out_size, "%s", mapped);
        acc_text_tidy(out);
        if (out[0] != '\0') {
            return;
        }
    }
    snprintf(out, out_size, "blank");
}

static void remember(const mED_Ovl_c* editor) {
    int len = editor->now_str_len;
    if (len < 0) {
        len = 0;
    }
    if (len > ACC_EDITOR_TEXT_MAX) {
        len = ACC_EDITOR_TEXT_MAX;
    }
    s_last.grid_col = editor->select_col;
    s_last.grid_row = editor->select_row;
    s_last.input_mode = editor->input_mode;
    s_last.shift_mode = editor->shift_mode;
    s_last.cursor_idx = editor->cursor_idx;
    s_last.length = (s16)len;
    s_last.live_text = editor->input_str;
    if (editor->input_str && len > 0) {
        memcpy(s_last.text, editor->input_str, (size_t)len);
    }
}

static void current_text(const mED_Ovl_c* editor, char* out, int out_size) {
    int len = editor->now_str_len;
    if (!editor->input_str || len <= 0) {
        out[0] = '\0';
        return;
    }
    acc_game_text_to_utf8(editor->input_str, len, out, out_size);
    acc_text_tidy(out);
}

void acc_text_editor_opened(const mED_Ovl_c* editor, int type, u8 grid_char) {
    char existing[ACC_EDITOR_TEXT_MAX * 2];
    char grid[32];
    char text[ACC_EDITOR_TEXT_MAX * 2 + 128];

    if (!editor) {
        return;
    }
    char title[72];
    snprintf(title, sizeof(title), "%s", s_pending_title[0] ? s_pending_title : editor_name(type));
    {
        size_t n = strlen(title);
        if (n > 0 && title[n - 1] != '.' && title[n - 1] != '!' && title[n - 1] != '?') {
            snprintf(title + n, sizeof(title) - n, ".");
        }
    }

    current_text(editor, existing, sizeof(existing));
    char_name(grid_char, grid, sizeof(grid));
    if (existing[0] != '\0') {
        snprintf(text, sizeof(text), "%s %s. Text: %s. %s", title,
                 mode_name(editor->input_mode, editor->shift_mode), existing, grid);
    } else {
        snprintf(text, sizeof(text), "%s %s, empty. %s", title,
                 mode_name(editor->input_mode, editor->shift_mode), grid);
    }
    s_pending_title[0] = '\0';
    acc_speak_interrupt(text);
    remember(editor);
    s_last.active = TRUE;
}

void acc_text_editor_update(const mED_Ovl_c* editor, u8 grid_char) {
    char name[32];
    char text[96];

    if (!editor || !s_last.active) {
        return;
    }

    /* Typing and deleting come first: typing on the PC keyboard can also
     * switch the grid's mode, and the typed character matters more. */
    if (editor->command_processed && editor->command == mED_COMMAND_OUTPUT_CODE) {
        char_name(editor->now_code, name, sizeof(name));
        acc_speak_interrupt(name);
    } else if (editor->command_processed && editor->command == mED_COMMAND_BACKSPACE) {
        /* The deleted character sat just before the old cursor. */
        int gone = s_last.cursor_idx - 1;
        if (gone >= 0 && gone < s_last.length) {
            char_name(s_last.text[gone], name, sizeof(name));
            snprintf(text, sizeof(text), "deleted %s", name);
        } else {
            snprintf(text, sizeof(text), "nothing to delete");
        }
        acc_speak_interrupt(text);
    } else if (editor->input_mode != s_last.input_mode || editor->shift_mode != s_last.shift_mode) {
        char_name(grid_char, name, sizeof(name));
        snprintf(text, sizeof(text), "%s. %s", mode_name(editor->input_mode, editor->shift_mode), name);
        acc_speak_interrupt(text);
    } else if (editor->cursor_idx != s_last.cursor_idx) {
        /* Text cursor moved: read the character it is now on. */
        if (editor->input_str && editor->cursor_idx >= 0 && editor->cursor_idx < editor->now_str_len) {
            char_name(editor->input_str[editor->cursor_idx], name, sizeof(name));
            acc_speak_interrupt(name);
        } else {
            acc_speak_interrupt("end of text");
        }
    } else if (editor->select_col != s_last.grid_col || editor->select_row != s_last.grid_row) {
        char_name(grid_char, name, sizeof(name));
        acc_speak_interrupt(name);
    }

    remember(editor);
}

void acc_text_editor_read_all(void) {
    char text[ACC_EDITOR_TEXT_MAX * 2];
    if (!s_last.active || s_last.length <= 0) {
        acc_speak_interrupt("empty");
        return;
    }
    acc_game_text_to_utf8(s_last.text, s_last.length, text, sizeof(text));
    acc_text_tidy(text);
    acc_speak_interrupt(text[0] ? text : "empty");
}

void acc_text_editor_closed(void) {
    s_last.active = FALSE;
}

int acc_text_editor_is_open(void) {
    return s_last.active;
}
