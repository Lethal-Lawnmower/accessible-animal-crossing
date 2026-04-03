/* pc_acc_menu.c - Menu narration for choice windows and inventory items.
 *
 * Choice windows: When a yes/no or multiple-choice menu appears during
 * dialogue, speak all options and the current selection. When the cursor
 * moves, speak the newly highlighted option.
 *
 * Inventory/tag items: When the player navigates to a new item in the
 * inventory grid or other tag-based menus, speak the item name. */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_msg.h"
#include "m_choice.h"
#include "m_editor_ovl.h"
#include "m_tag_ovl.h"
#include "m_submenu.h"
#include "m_mail.h"
#include "m_font.h"
#include "m_timeIn_ovl.h"
#include "m_map_ovl.h"
#include "m_notice.h"
#include "m_common_data.h"
#include "lb_rtc.h"

#include <string.h>
#include <stdio.h>

/* Buffer for building TTS strings */
#define ACC_MENU_BUF_SIZE 512

/* ========================================================================= */
/* Choice window narration                                                   */
/* ========================================================================= */

/* Track which choice index was last spoken to avoid repeating */
static int s_last_choice_idx = -1;

void pc_acc_choice_appeared(mChoice_c* choice) {
    if (!pc_acc_is_active()) return;
    if (!choice || choice->data.choice_num <= 0) return;

    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;
    int num = choice->data.choice_num;
    int sel = choice->selected_choice_idx;

    for (int i = 0; i < num && oi < ACC_MENU_BUF_SIZE - 64; i++) {
        /* Convert game-encoded choice string to UTF-8 */
        char item_buf[64];
        int len = choice->data.string_lens[i];
        if (len <= 0 || len > mChoice_CHOICE_STRING_LEN) continue;

        /* Debug: dump raw bytes */
        printf("[ACC_MENU] Choice %d/%d len=%d bytes:", i, num, len);
        for (int d = 0; d < len; d++) printf(" %02X", choice->data.strings[i][d]);
        printf("\n");

        int wrote = pc_acc_game_str_to_utf8(choice->data.strings[i], len, item_buf, sizeof(item_buf));

        if (wrote > 0) {
            if (oi > 0) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, ", ");
            }
            if (i == sel) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s, selected", item_buf);
            } else {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s", item_buf);
            }
        }
    }

    if (oi > 0) {
        pc_acc_speak_interrupt(buf);
    }

    s_last_choice_idx = sel;
}

void pc_acc_choice_update(mChoice_c* choice) {
    if (!pc_acc_is_active()) return;
    if (!choice || choice->data.choice_num <= 0) return;

    int sel = choice->selected_choice_idx;
    if (sel == s_last_choice_idx) return;

    s_last_choice_idx = sel;

    char buf[64];
    int len = choice->data.string_lens[sel];
    if (len <= 0 || len > mChoice_CHOICE_STRING_LEN) return;
    int wrote = pc_acc_game_str_to_utf8(choice->data.strings[sel], len, buf, sizeof(buf));

    if (wrote > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

/* ========================================================================= */
/* Inventory / tag item narration                                            */
/* ========================================================================= */

void pc_acc_tag_item_changed(const u8* tag_str0, int max_len, int table) {
    if (!pc_acc_is_active()) return;
    if (!tag_str0) return;

    /* Find effective length (game strings are padded with CHAR_SPACE) */
    int len = mMl_strlen((u8*)tag_str0, max_len, CHAR_SPACE);
    if (len <= 0) {
        /* Player character slot shows blank when nothing equipped */
        if (table == mTG_TABLE_PLAYER) {
            pc_acc_speak_interrupt("Your character");
            return;
        }
        pc_acc_speak_interrupt("Empty");
        return;
    }

    /* Player character slot with equipment shows item name, prefix with context */
    if (table == mTG_TABLE_PLAYER) {
        char item_buf[128];
        int wrote = pc_acc_game_str_to_utf8(tag_str0, len, item_buf, sizeof(item_buf));
        if (wrote > 0) {
            char buf[160];
            snprintf(buf, sizeof(buf), "Your character, holding %s", item_buf);
            pc_acc_speak_interrupt(buf);
        }
        return;
    }

    char buf[128];
    int wrote = pc_acc_game_str_to_utf8(tag_str0, len, buf, sizeof(buf));

    if (wrote > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

/* ========================================================================= */
/* Editor / keyboard narration                                               */
/* ========================================================================= */

static u8 s_last_editor_col = 0xFF;
static u8 s_last_editor_row = 0xFF;
static u8 s_last_editor_command = 0xFF;
static u8 s_last_editor_shift = 0xFF;
static u8 s_last_editor_input_mode = 0xFF;
static s16 s_last_editor_str_len = -1;

/* Speak a single game-encoded character byte */
static void mED_speak_char(u8 game_char) {
    char buf[16];
    int wrote = pc_acc_game_str_to_utf8(&game_char, 1, buf, sizeof(buf));
    if (wrote > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

static const char* mED_mode_name(int input_mode, int shift_mode) {
    if (input_mode == mED_INPUT_MODE_SIGN) return "symbols";
    if (input_mode == mED_INPUT_MODE_MARK) return "marks";
    if (shift_mode == mED_SHIFT_UPPER) return "uppercase";
    return "lowercase";
}

void pc_acc_editor_opened(mED_Ovl_c* editor_ovl, u8 grid_char) {
    if (!pc_acc_is_active()) return;
    if (!editor_ovl) return;

    char buf[128];
    char char_buf[16];
    int wrote = pc_acc_game_str_to_utf8(&grid_char, 1, char_buf, sizeof(char_buf));

    if (wrote > 0) {
        snprintf(buf, sizeof(buf), "Keyboard, %s. %s",
                 mED_mode_name(editor_ovl->input_mode, editor_ovl->shift_mode),
                 char_buf);
    } else {
        snprintf(buf, sizeof(buf), "Keyboard, %s",
                 mED_mode_name(editor_ovl->input_mode, editor_ovl->shift_mode));
    }

    pc_acc_speak_interrupt(buf);

    s_last_editor_col = editor_ovl->select_col;
    s_last_editor_row = editor_ovl->select_row;
    s_last_editor_command = mED_COMMAND_NONE;
    s_last_editor_shift = editor_ovl->shift_mode;
    s_last_editor_input_mode = editor_ovl->input_mode;
    s_last_editor_str_len = editor_ovl->now_str_len;
}

void pc_acc_editor_update(mED_Ovl_c* editor_ovl, u8 grid_char) {
    if (!pc_acc_is_active()) return;
    if (!editor_ovl) return;

    /* Check for mode/shift changes first */
    if (editor_ovl->input_mode != s_last_editor_input_mode ||
        editor_ovl->shift_mode != s_last_editor_shift) {
        char buf[64];
        char char_buf[16];
        int wrote = pc_acc_game_str_to_utf8(&grid_char, 1, char_buf, sizeof(char_buf));

        snprintf(buf, sizeof(buf), "%s. %s",
                 mED_mode_name(editor_ovl->input_mode, editor_ovl->shift_mode),
                 wrote > 0 ? char_buf : "");
        pc_acc_speak_interrupt(buf);

        s_last_editor_input_mode = editor_ovl->input_mode;
        s_last_editor_shift = editor_ovl->shift_mode;
        s_last_editor_col = editor_ovl->select_col;
        s_last_editor_row = editor_ovl->select_row;
        s_last_editor_str_len = editor_ovl->now_str_len;
        return;
    }

    /* Character was typed */
    if (editor_ovl->command == mED_COMMAND_OUTPUT_CODE && editor_ovl->command_processed) {
        char buf[16];
        u8 code = editor_ovl->now_code;
        int wrote = pc_acc_game_str_to_utf8(&code, 1, buf, sizeof(buf));
        if (wrote > 0) {
            pc_acc_speak_interrupt(buf);
        }
        s_last_editor_col = editor_ovl->select_col;
        s_last_editor_row = editor_ovl->select_row;
        s_last_editor_str_len = editor_ovl->now_str_len;
        return;
    }

    /* Backspace */
    if (editor_ovl->command == mED_COMMAND_BACKSPACE && editor_ovl->command_processed) {
        pc_acc_speak_interrupt("delete");
        s_last_editor_str_len = editor_ovl->now_str_len;
        return;
    }

    /* Grid cursor moved — speak the character under the new position */
    if (editor_ovl->select_col != s_last_editor_col ||
        editor_ovl->select_row != s_last_editor_row) {
        mED_speak_char(grid_char);
        s_last_editor_col = editor_ovl->select_col;
        s_last_editor_row = editor_ovl->select_row;
    }

    s_last_editor_str_len = editor_ovl->now_str_len;
}

/* ========================================================================= */
/* TimeIn (clock adjustment) narration                                       */
/* ========================================================================= */

static int s_last_timein_sel = -1;
static u16 s_last_timein_values[mTI_IDX_NUM];

static const char* mTI_field_names[] = {
    "Hour", "Minute", "Month", "Day", "Year"
};

static const char* mTI_month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static void mTI_speak_field(int sel_idx, const u16* values, int month) {
    char buf[128];

    if (sel_idx == mTI_IDX_OK) {
        snprintf(buf, sizeof(buf), "OK");
    } else if (sel_idx == mTI_IDX_MONTH) {
        int m = month;
        if (m >= 1 && m <= 12) {
            snprintf(buf, sizeof(buf), "Month: %s", mTI_month_names[m - 1]);
        } else {
            snprintf(buf, sizeof(buf), "Month: %d", m);
        }
    } else if (sel_idx == mTI_IDX_YEAR) {
        snprintf(buf, sizeof(buf), "Year: %d", 2000 + values[mTI_IDX_YEAR]);
    } else {
        snprintf(buf, sizeof(buf), "%s: %d", mTI_field_names[sel_idx], values[sel_idx]);
    }

    pc_acc_speak_interrupt(buf);
}

void pc_acc_timein_opened(int sel_idx, const u16* values, int month) {
    if (!pc_acc_is_active()) return;

    /* Speak the title first, then the current field */
    char buf[256];
    const char* field;

    if (sel_idx == mTI_IDX_OK) {
        field = "OK";
    } else if (sel_idx == mTI_IDX_MONTH) {
        int m = month;
        static char mbuf[32];
        if (m >= 1 && m <= 12)
            snprintf(mbuf, sizeof(mbuf), "Month: %s", mTI_month_names[m - 1]);
        else
            snprintf(mbuf, sizeof(mbuf), "Month: %d", m);
        field = mbuf;
    } else if (sel_idx == mTI_IDX_YEAR) {
        static char ybuf[32];
        snprintf(ybuf, sizeof(ybuf), "Year: %d", 2000 + values[mTI_IDX_YEAR]);
        field = ybuf;
    } else {
        static char fbuf[32];
        snprintf(fbuf, sizeof(fbuf), "%s: %d", mTI_field_names[sel_idx], values[sel_idx]);
        field = fbuf;
    }

    snprintf(buf, sizeof(buf), "Adjust the clock. %s", field);
    pc_acc_speak_interrupt(buf);

    s_last_timein_sel = sel_idx;
    memcpy(s_last_timein_values, values, sizeof(u16) * mTI_IDX_NUM);
}

void pc_acc_timein_update(int sel_idx, const u16* values, int month) {
    if (!pc_acc_is_active()) return;

    int sel_changed = (sel_idx != s_last_timein_sel);
    int val_changed = 0;

    for (int i = 0; i < mTI_IDX_NUM; i++) {
        if (values[i] != s_last_timein_values[i]) {
            val_changed = 1;
            break;
        }
    }

    if (!sel_changed && !val_changed) return;

    mTI_speak_field(sel_idx, values, month);

    s_last_timein_sel = sel_idx;
    memcpy(s_last_timein_values, values, sizeof(u16) * mTI_IDX_NUM);
}

/* ========================================================================= */
/* Tag action popup narration (Drop / Give / Grab / etc.)                    */
/* ========================================================================= */

void pc_acc_tag_action_opened(const u8* const* action_strs, int num_actions, int sel_row, int str_len) {
    if (!pc_acc_is_active()) return;
    if (!action_strs || num_actions <= 0) return;

    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;

    for (int i = 0; i < num_actions && oi < ACC_MENU_BUF_SIZE - 64; i++) {
        if (!action_strs[i]) continue;

        char item_buf[32];
        int len = mMl_strlen((u8*)action_strs[i], str_len, CHAR_SPACE);
        if (len <= 0) continue;

        int wrote = pc_acc_game_str_to_utf8(action_strs[i], len, item_buf, sizeof(item_buf));
        if (wrote > 0) {
            if (oi > 0) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, ", ");
            }
            if (i == sel_row) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s, selected", item_buf);
            } else {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s", item_buf);
            }
        }
    }

    if (oi > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

void pc_acc_tag_action_update(const u8* action_str, int str_len) {
    if (!pc_acc_is_active()) return;
    if (!action_str) return;

    int len = mMl_strlen((u8*)action_str, str_len, CHAR_SPACE);
    if (len <= 0) return;

    char buf[32];
    int wrote = pc_acc_game_str_to_utf8(action_str, len, buf, sizeof(buf));
    if (wrote > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

/* ========================================================================= */
/* Inventory section announcements                                           */
/* ========================================================================= */

static const char* table_section_name(int table) {
    switch (table) {
        case mTG_TABLE_ITEM:             return "Pockets";
        case mTG_TABLE_MAIL:             return "Letters";
        case mTG_TABLE_MONEY:            return "Money";
        case mTG_TABLE_PLAYER:           return "Equipped";
        case mTG_TABLE_BG:               return "Wallpaper and Carpet";
        case mTG_TABLE_MBOX:             return "Mailbox";
        case mTG_TABLE_HANIWA:           return "Gyroid Storage";
        case mTG_TABLE_COLLECT:          return "Collection";
        case mTG_TABLE_WCHANGE:          return "Page";
        case mTG_TABLE_CATALOG:          return "Catalog";
        case mTG_TABLE_MUSIC_MAIN:       return "Music";
        case mTG_TABLE_NEEDLEWORK:       return "Designs";
        case mTG_TABLE_INVENTORY_WC_ORG: return "Designs";
        default:                         return NULL;
    }
}

void pc_acc_tag_table_changed(int table) {
    if (!pc_acc_is_active()) return;

    const char* name = table_section_name(table);
    if (name) {
        pc_acc_speak_interrupt(name);
    }
}

void pc_acc_submenu_opened(int menu_type) {
    if (!pc_acc_is_active()) return;

    const char* name = NULL;
    switch (menu_type) {
        case mSM_OVL_INVENTORY:    name = "Inventory"; break;
        case mSM_OVL_MAP:          name = "Map"; break;
        case mSM_OVL_MAILBOX:      name = "Mailbox"; break;
        case mSM_OVL_HANIWA:       name = "Gyroid Storage"; break;
        case mSM_OVL_CATALOG:      name = "Catalog"; break;
        case mSM_OVL_MUSIC:        name = "Music"; break;
        case mSM_OVL_NEEDLEWORK:   name = "Able Sisters"; break;
        case mSM_OVL_BANK:         name = "Bank"; break;
        default: break;
    }

    if (name) {
        pc_acc_speak_interrupt(name);
    }
}

/* ========================================================================= */
/* Address book narration                                                    */
/* ========================================================================= */

void pc_acc_address_selection(const u8* name, int name_len, int entry_idx, int entry_count, int page_idx) {
    if (!pc_acc_is_active()) return;

    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;

    if (name && name_len > 0) {
        int len = mMl_strlen((u8*)name, name_len, CHAR_SPACE);
        if (len > 0) {
            char name_buf[16];
            int wrote = pc_acc_game_str_to_utf8(name, len, name_buf, sizeof(name_buf));
            if (wrote > 0) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s", name_buf);
            }
        }
    }

    oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, ", %d of %d", entry_idx + 1, entry_count);

    if (oi > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

/* ========================================================================= */
/* Town map narration                                                        */
/* ========================================================================= */

static const char* map_label_name(int label_no) {
    switch (label_no) {
        case mMP_LABEL_NPC:     return NULL; /* residents spoken separately */
        case mMP_LABEL_PLAYER:  return NULL; /* residents spoken separately */
        case mMP_LABEL_SHOP:    return "Shop";
        case mMP_LABEL_POLICE:  return "Police Station";
        case mMP_LABEL_POST:    return "Post Office";
        case mMP_LABEL_SHRINE:  return "Wishing Well";
        case mMP_LABEL_STATION: return "Train Station";
        case mMP_LABEL_JUNK:    return "Dump";
        case mMP_LABEL_MUSEUM:  return "Museum";
        case mMP_LABEL_NEEDLE:  return "Tailor";
        case mMP_LABEL_PORT:    return "Dock";
        default:                return NULL;
    }
}

static void map_speak_acre(char* buf, int buf_size, int sel_bx, int sel_bz,
                           int player_bx, int player_bz,
                           int label_no, int label_cnt,
                           const u8* resident_names[], int resident_count) {
    int oi = 0;

    /* Acre label: "Acre A-1" */
    oi += snprintf(buf + oi, buf_size - oi, "Acre %c-%d", 'A' + sel_bz, sel_bx + 1);

    /* Player marker */
    if (sel_bx == player_bx && sel_bz == player_bz) {
        oi += snprintf(buf + oi, buf_size - oi, ", you are here");
    }

    /* Building name if present */
    const char* building = map_label_name(label_no);
    if (building) {
        oi += snprintf(buf + oi, buf_size - oi, ", %s", building);
    }

    /* Resident names */
    for (int i = 0; i < resident_count && i < label_cnt && oi < buf_size - 40; i++) {
        if (!resident_names[i]) continue;
        char name_buf[16];
        int len = mMl_strlen((u8*)resident_names[i], PLAYER_NAME_LEN, CHAR_SPACE);
        if (len <= 0) continue;
        int wrote = pc_acc_game_str_to_utf8(resident_names[i], len, name_buf, sizeof(name_buf));
        if (wrote > 0) {
            oi += snprintf(buf + oi, buf_size - oi, ", %s", name_buf);
        }
    }

    pc_acc_speak_interrupt(buf);
}

void pc_acc_map_opened(int sel_bx, int sel_bz, int player_bx, int player_bz,
                       const u8* land_name, int land_name_len) {
    if (!pc_acc_is_active()) return;

    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;

    /* Town name */
    if (land_name && land_name_len > 0) {
        char town_buf[16];
        int wrote = pc_acc_game_str_to_utf8(land_name, land_name_len, town_buf, sizeof(town_buf));
        if (wrote > 0) {
            oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s map. ", town_buf);
        }
    }

    oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "Acre %c-%d, you are here",
                   'A' + sel_bz, sel_bx + 1);

    pc_acc_speak_interrupt(buf);
}

void pc_acc_map_cursor_moved(int sel_bx, int sel_bz, int player_bx, int player_bz,
                             int label_no, int label_cnt,
                             const u8* resident_names[], int resident_count) {
    if (!pc_acc_is_active()) return;

    char buf[ACC_MENU_BUF_SIZE];
    map_speak_acre(buf, ACC_MENU_BUF_SIZE, sel_bx, sel_bz, player_bx, player_bz,
                   label_no, label_cnt, resident_names, resident_count);
}

/* ========================================================================= */
/* Notice board / bulletin board narration                                   */
/* ========================================================================= */

static const char* month_name(int month) {
    static const char* names[] = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    if (month >= 1 && month <= 12) return names[month];
    return "";
}

static void notice_speak_entry(int now_page, int page_count,
                               const u8* message, int msg_len,
                               const lbRTC_time_c* post_time) {
    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;

    /* "Entry 5 of 8" */
    oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "Entry %d of %d. ",
                   now_page + 1, page_count);

    /* Date */
    if (post_time) {
        oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s %d, %d. ",
                       month_name(post_time->month), post_time->day, post_time->year);
    }

    /* Message text */
    if (message && msg_len > 0) {
        int len = mMl_strlen((u8*)message, msg_len, CHAR_SPACE);
        if (len > 0) {
            char msg_buf[384];
            int wrote = pc_acc_game_str_to_utf8(message, len, msg_buf, sizeof(msg_buf));
            if (wrote > 0) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s", msg_buf);
            }
        }
    }

    if (oi > 0) {
        pc_acc_speak_interrupt(buf);
    }
}

void pc_acc_notice_opened(int now_page, int page_count,
                          const u8* message, int msg_len,
                          const lbRTC_time_c* post_time) {
    if (!pc_acc_is_active()) return;

    if (page_count <= 0) {
        pc_acc_speak_interrupt("Bulletin Board. No entries.");
        return;
    }

    /* Build combined announcement: "Bulletin Board. Entry N of M. ..." */
    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;
    oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "Bulletin Board. Entry %d of %d. ",
                   now_page + 1, page_count);

    if (post_time) {
        oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s %d, %d. ",
                       month_name(post_time->month), post_time->day, post_time->year);
    }

    if (message && msg_len > 0) {
        int len = mMl_strlen((u8*)message, msg_len, CHAR_SPACE);
        if (len > 0) {
            char msg_buf[384];
            int wrote = pc_acc_game_str_to_utf8(message, len, msg_buf, sizeof(msg_buf));
            if (wrote > 0) {
                oi += snprintf(buf + oi, ACC_MENU_BUF_SIZE - oi, "%s", msg_buf);
            }
        }
    }

    pc_acc_speak_interrupt(buf);
}

void pc_acc_notice_page_changed(int now_page, int page_count,
                                const u8* message, int msg_len,
                                const lbRTC_time_c* post_time) {
    if (!pc_acc_is_active()) return;
    notice_speak_entry(now_page, page_count, message, msg_len, post_time);
}

/* ========================================================================= */
/* Letter / board overlay narration                                          */
/* ========================================================================= */

void pc_acc_board_opened(const u8* header, int header_len,
                         const u8* body, int body_len,
                         const u8* footer, int footer_len) {
    if (!pc_acc_is_active()) return;

    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;

    if (header && header_len > 0) {
        int len = mMl_strlen((u8*)header, header_len, CHAR_SPACE);
        if (len > 0) {
            int wrote = pc_acc_game_str_to_utf8(header, len, buf + oi, ACC_MENU_BUF_SIZE - oi);
            if (wrote > 0) oi += wrote;
        }
    }

    if (body && body_len > 0) {
        int len = mMl_strlen((u8*)body, body_len, CHAR_SPACE);
        if (len > 0) {
            if (oi > 0 && oi < ACC_MENU_BUF_SIZE - 2) {
                buf[oi++] = ' ';
            }
            int wrote = pc_acc_game_str_to_utf8(body, len, buf + oi, ACC_MENU_BUF_SIZE - oi);
            if (wrote > 0) oi += wrote;
        }
    }

    if (footer && footer_len > 0) {
        int len = mMl_strlen((u8*)footer, footer_len, CHAR_SPACE);
        if (len > 0) {
            if (oi > 0 && oi < ACC_MENU_BUF_SIZE - 2) {
                buf[oi++] = ' ';
            }
            int wrote = pc_acc_game_str_to_utf8(footer, len, buf + oi, ACC_MENU_BUF_SIZE - oi);
            if (wrote > 0) oi += wrote;
        }
    }

    if (oi > 0) {
        buf[oi] = '\0';
        pc_acc_speak_interrupt(buf);
    }
}

/* ========================================================================= */
/* Bank overlay narration                                                    */
/* ========================================================================= */

static int s_last_bank_cursol = -1;
static int s_last_bank_bell = -1;

void pc_acc_bank_opened(int wallet, int account) {
    if (!pc_acc_is_active()) return;

    char buf[128];
    snprintf(buf, sizeof(buf), "Bank. Wallet: %d Bells. Account: %d Bells.", wallet, account);
    pc_acc_speak_interrupt(buf);

    s_last_bank_cursol = -1;
    s_last_bank_bell = -1;
}

void pc_acc_bank_update(int cursol, int wallet, int account, int bell) {
    if (!pc_acc_is_active()) return;

    int cursol_changed = (cursol != s_last_bank_cursol);
    int bell_changed = (bell != s_last_bank_bell);

    if (!cursol_changed && !bell_changed) return;

    char buf[128];

    if (bell_changed) {
        snprintf(buf, sizeof(buf), "Transfer: %d. Wallet: %d. Account: %d.", bell, wallet, account);
    } else if (cursol == 6) {
        snprintf(buf, sizeof(buf), "OK");
    } else {
        /* Cursor moved to a different digit position */
        static const char* digit_names[] = {
            "hundred thousands", "ten thousands", "thousands", "hundreds", "tens", "ones"
        };
        if (cursol >= 0 && cursol <= 5) {
            snprintf(buf, sizeof(buf), "%s", digit_names[cursol]);
        } else {
            buf[0] = '\0';
        }
    }

    if (buf[0] != '\0') {
        pc_acc_speak_interrupt(buf);
    }

    s_last_bank_cursol = cursol;
    s_last_bank_bell = bell;
}

/* ========================================================================= */
/* Loan repayment overlay narration                                          */
/* ========================================================================= */

static int s_last_repay_cursor = -1;
static u32 s_last_repay_amount = 0xFFFFFFFF;

void pc_acc_repay_opened(u32 money, u32 loan) {
    if (!pc_acc_is_active()) return;

    char buf[128];
    snprintf(buf, sizeof(buf), "Loan Repayment. Your money: %lu. Remaining loan: %lu.", (unsigned long)money, (unsigned long)loan);
    pc_acc_speak_interrupt(buf);

    s_last_repay_cursor = -1;
    s_last_repay_amount = 0xFFFFFFFF;
}

void pc_acc_repay_update(int cursor_idx, u32 repay_amount, u32 money, u32 loan) {
    if (!pc_acc_is_active()) return;

    int cursor_changed = (cursor_idx != s_last_repay_cursor);
    int amount_changed = (repay_amount != s_last_repay_amount);

    if (!cursor_changed && !amount_changed) return;

    char buf[128];

    if (amount_changed) {
        snprintf(buf, sizeof(buf), "Repay: %lu. Remaining: %lu. Money left: %lu.", (unsigned long)repay_amount, (unsigned long)loan, (unsigned long)money);
    } else if (cursor_idx == 6) {
        snprintf(buf, sizeof(buf), "OK");
    } else {
        static const char* digit_names[] = {
            "hundred thousands", "ten thousands", "thousands", "hundreds", "tens", "ones"
        };
        if (cursor_idx >= 0 && cursor_idx <= 5) {
            snprintf(buf, sizeof(buf), "%s", digit_names[cursor_idx]);
        } else {
            buf[0] = '\0';
        }
    }

    if (buf[0] != '\0') {
        pc_acc_speak_interrupt(buf);
    }

    s_last_repay_cursor = cursor_idx;
    s_last_repay_amount = repay_amount;
}

/* ========================================================================= */
/* Warning popup narration                                                   */
/* ========================================================================= */

void pc_acc_warning_opened(int warning_type, const u8* const* line_strs, const int* line_lens, int num_lines) {
    if (!pc_acc_is_active()) return;
    if (num_lines <= 0) return;

    char buf[ACC_MENU_BUF_SIZE];
    int oi = 0;

    for (int i = 0; i < num_lines; i++) {
        if (!line_strs[i] || line_lens[i] <= 0) continue;

        if (oi > 0 && oi < ACC_MENU_BUF_SIZE - 2) {
            buf[oi++] = ' ';
        }

        int wrote = pc_acc_game_str_to_utf8(line_strs[i], line_lens[i], buf + oi, ACC_MENU_BUF_SIZE - oi);
        if (wrote > 0) {
            oi += wrote;
        }
    }

    if (oi > 0) {
        buf[oi] = '\0';
        pc_acc_speak_interrupt(buf);
    }
}

#else
/* Non-Windows stubs */
#include "m_choice.h"
#include "m_editor_ovl.h"
#include "lb_rtc.h"
void pc_acc_choice_appeared(mChoice_c* choice) { (void)choice; }
void pc_acc_choice_update(mChoice_c* choice) { (void)choice; }
void pc_acc_tag_item_changed(const u8* tag_str0, int max_len, int table) { (void)tag_str0; (void)max_len; (void)table; }
void pc_acc_editor_opened(mED_Ovl_c* editor_ovl, u8 grid_char) { (void)editor_ovl; (void)grid_char; }
void pc_acc_editor_update(mED_Ovl_c* editor_ovl, u8 grid_char) { (void)editor_ovl; (void)grid_char; }
void pc_acc_timein_opened(int sel_idx, const u16* values, int month) { (void)sel_idx; (void)values; (void)month; }
void pc_acc_timein_update(int sel_idx, const u16* values, int month) { (void)sel_idx; (void)values; (void)month; }
void pc_acc_tag_action_opened(const u8* const* a, int n, int s, int l) { (void)a; (void)n; (void)s; (void)l; }
void pc_acc_tag_action_update(const u8* a, int l) { (void)a; (void)l; }
void pc_acc_tag_table_changed(int t) { (void)t; }
void pc_acc_submenu_opened(int t) { (void)t; }
void pc_acc_address_selection(const u8* a, int b, int c, int d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
void pc_acc_map_opened(int a, int b, int c, int d, const u8* e, int f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void pc_acc_map_cursor_moved(int a, int b, int c, int d, int e, int f, const u8* g[], int h) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; }
void pc_acc_notice_opened(int a, int b, const u8* c, int d, const lbRTC_time_c* e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
void pc_acc_notice_page_changed(int a, int b, const u8* c, int d, const lbRTC_time_c* e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
void pc_acc_warning_opened(int a, const u8* const* b, const int* c, int d) { (void)a; (void)b; (void)c; (void)d; }
void pc_acc_board_opened(const u8* a, int b, const u8* c, int d, const u8* e, int f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void pc_acc_bank_opened(int a, int b) { (void)a; (void)b; }
void pc_acc_bank_update(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void pc_acc_repay_opened(u32 a, u32 b) { (void)a; (void)b; }
void pc_acc_repay_update(int a, u32 b, u32 c, u32 d) { (void)a; (void)b; (void)c; (void)d; }
#endif
