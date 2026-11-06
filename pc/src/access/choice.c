/* choice.c - Choice window speech. See access/choice.h.
 *
 * Choices usually appear right after the question in the message window.
 * The option list is queued rather than interrupting, so the question is
 * not cut off; moving the selection interrupts, so only the current option
 * is heard when browsing. (The previous project interrupted on open.) */

#include "access/choice.h"

#include "access/game_text.h"
#include "access/speech.h"

#include <stdio.h>

static int s_last_index = -1;

static void option_text(const mChoice_c* choice, int index, char* out, int out_size) {
    int len = choice->data.string_lens[index];
    if (len < 0) {
        len = 0;
    }
    if (len > mChoice_CHOICE_STRING_LEN) {
        len = mChoice_CHOICE_STRING_LEN;
    }
    acc_game_field_to_utf8(choice->data.strings[index], len, out, out_size);
}

static int option_count(const mChoice_c* choice) {
    int count = choice->data.choice_num;
    if (count < 0) {
        return 0;
    }
    return count > mChoice_CHOICE_NUM ? mChoice_CHOICE_NUM : count;
}

static void speak_selected(const mChoice_c* choice, int interrupt) {
    char label[64];
    char text[128];
    int count = option_count(choice);
    int index = choice->selected_choice_idx;
    if (index < 0 || index >= count) {
        return;
    }
    option_text(choice, index, label, sizeof(label));
    if (label[0] == '\0') {
        return;
    }
    snprintf(text, sizeof(text), "%s, %d of %d", label, index + 1, count);
    acc_speak(text, interrupt != 0);
}

void acc_choice_opened(mChoice_c* choice) {
    char list[512];
    int pos = 0;
    int count = choice ? option_count(choice) : 0;

    s_last_index = -1;
    if (count <= 0) {
        return;
    }
    list[0] = '\0';
    acc_text_append(list, sizeof(list), &pos, "Choices: ");
    for (int i = 0; i < count; i++) {
        char label[64];
        option_text(choice, i, label, sizeof(label));
        if (label[0] == '\0') {
            continue;
        }
        if (i > 0) {
            acc_text_append(list, sizeof(list), &pos, ", ");
        }
        acc_text_append(list, sizeof(list), &pos, label);
    }
    /* End the list with a period unless the last option already ends in
     * punctuation ("Before I go..."). */
    if (pos > 0 && list[pos - 1] != '.' && list[pos - 1] != '!' && list[pos - 1] != '?') {
        acc_text_append(list, sizeof(list), &pos, ".");
    }
    acc_speak_queue(list);
    speak_selected(choice, 0);
    s_last_index = choice->selected_choice_idx;
}

void acc_choice_update(mChoice_c* choice) {
    if (!choice || choice->selected_choice_idx == s_last_index) {
        return;
    }
    s_last_index = choice->selected_choice_idx;
    speak_selected(choice, 1);
}
