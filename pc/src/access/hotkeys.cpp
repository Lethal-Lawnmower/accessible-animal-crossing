// hotkeys.cpp - Accessibility keyboard shortcuts. See access/hotkeys.h.

#include "access/hotkeys.h"

#include "access/speech.h"
// From access/text_editor.h, which pulls in game headers that do not
// compile as C++, so the two functions used here are declared directly.
extern "C" {
int acc_text_editor_is_open(void);
void acc_text_editor_read_all(void);
}

void acc_hotkeys_handle_event(const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
        return;
    }
    switch (event.key.key) {
        case SDLK_F5:
            if (acc_text_editor_is_open()) {
                acc_text_editor_read_all();
            } else {
                acc_speech_repeat();
            }
            break;
        default:
            break;
    }
}
