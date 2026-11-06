/* item_menu.c - Item menu (tag overlay) speech. See access/item_menu.h.
 *
 * Carried over from the previous project's pc_acc_menu.c, with changes:
 *  - Slots announce their position ("3 of 15"); empty slots say "Empty".
 *  - All three label lines are read, not only the first, so details the game
 *    shows (for example a letter's sender) are not lost.
 *  - Menu and section names no longer get cut off by the slot announcement
 *    that immediately follows them.
 *  - Menu names cover every submenu that uses this overlay. */

#include "access/item_menu.h"

#include "access/game_text.h"
#include "access/speech.h"

#include "m_submenu.h"
#include "m_tag_ovl.h"

#include <stdio.h>

/* Set when a menu or section name was just spoken; the next slot
 * announcement then queues behind it instead of interrupting it. */
static int s_context_pending = FALSE;

static const char* section_name(int table) {
    switch (table) {
        case mTG_TABLE_ITEM:             return "Pockets";
        case mTG_TABLE_MAIL:             return "Letters";
        case mTG_TABLE_MONEY:            return "Bells";
        case mTG_TABLE_PLAYER:           return "Your character";
        case mTG_TABLE_BG:               return "Wallpaper and carpet";
        case mTG_TABLE_MBOX:             return "Mailbox";
        case mTG_TABLE_HANIWA:           return "Gyroid storage";
        case mTG_TABLE_COLLECT:          return "Collection";
        case mTG_TABLE_WCHANGE:          return "Page";
        case mTG_TABLE_CATALOG:          return "Catalog";
        case mTG_TABLE_MUSIC_MAIN:       return "Music";
        case mTG_TABLE_NEEDLEWORK:       return "Designs";
        case mTG_TABLE_INVENTORY_WC_ORG: return "Designs";
        default:                         return NULL;
    }
}

static const char* menu_name(int menu_type) {
    switch (menu_type) {
        case mSM_OVL_INVENTORY:  return "Inventory";
        case mSM_OVL_MAILBOX:    return "Mailbox";
        case mSM_OVL_HANIWA:     return "Gyroid storage";
        case mSM_OVL_CATALOG:    return "Catalog";
        case mSM_OVL_MUSIC:      return "Music";
        case mSM_OVL_NEEDLEWORK: return "Designs";
        case mSM_OVL_CPMAIL:     return "Memory card letters";
        case mSM_OVL_CPORIGINAL: return "Memory card designs";
        case mSM_OVL_GBA:        return "Game Boy Advance designs";
        default:                 return NULL;
    }
}

void acc_item_menu_opened(int menu_type) {
    const char* name = menu_name(menu_type);
    if (name) {
        acc_speak_interrupt(name);
        s_context_pending = TRUE;
    }
}

void acc_item_section_changed(int table) {
    const char* name = section_name(table);
    if (name) {
        acc_speak_interrupt(name);
        s_context_pending = TRUE;
    }
}

void acc_item_slot_focused(int table, int index, int count, const u8* line0, const u8* line1, const u8* line2,
                           int line_len) {
    const u8* lines[3] = {line0, line1, line2};
    char text[256];
    int pos = 0;

    text[0] = '\0';
    for (int i = 0; i < 3; i++) {
        char part[96];
        if (!lines[i]) {
            continue;
        }
        acc_game_field_to_utf8(lines[i], line_len, part, sizeof(part));
        if (part[0] == '\0') {
            continue;
        }
        if (pos > 0) {
            acc_text_append(text, sizeof(text), &pos, ", ");
        }
        acc_text_append(text, sizeof(text), &pos, part);
    }

    if (table == mTG_TABLE_PLAYER) {
        /* The character slot shows what the player is holding, if anything. */
        char held[256];
        if (pos > 0) {
            snprintf(held, sizeof(held), "Your character, holding %s", text);
        } else {
            snprintf(held, sizeof(held), "Your character");
        }
        snprintf(text, sizeof(text), "%s", held);
    } else {
        if (pos == 0) {
            acc_text_append(text, sizeof(text), &pos, "Empty");
        }
        if (count > 1 && index >= 0 && index < count) {
            char where[32];
            snprintf(where, sizeof(where), ", %d of %d", index + 1, count);
            acc_text_append(text, sizeof(text), &pos, where);
        }
    }

    acc_speak(text, !s_context_pending);
    s_context_pending = FALSE;
}

static void speak_action(const u8* label, int label_len, int index, int count, int interrupt) {
    char name[48];
    char text[96];
    acc_game_field_to_utf8(label, label_len, name, sizeof(name));
    if (name[0] == '\0') {
        return;
    }
    snprintf(text, sizeof(text), "%s, %d of %d", name, index + 1, count);
    acc_speak(text, interrupt != 0);
}

void acc_item_actions_opened(const u8* const* labels, int count, int selected, int label_len) {
    char text[256];
    int pos = 0;

    text[0] = '\0';
    acc_text_append(text, sizeof(text), &pos, "Actions: ");
    for (int i = 0; i < count; i++) {
        char label[48];
        if (!labels[i]) {
            continue;
        }
        acc_game_field_to_utf8(labels[i], label_len, label, sizeof(label));
        if (label[0] == '\0') {
            continue;
        }
        if (i > 0) {
            acc_text_append(text, sizeof(text), &pos, ", ");
        }
        acc_text_append(text, sizeof(text), &pos, label);
    }
    acc_speak_interrupt(text);
    if (selected >= 0 && selected < count && labels[selected]) {
        /* Queue the highlighted action after the list. */
        speak_action(labels[selected], label_len, selected, count, FALSE);
    }
}

void acc_item_action_moved(const u8* label, int label_len, int index, int count) {
    speak_action(label, label_len, index, count, TRUE);
}
