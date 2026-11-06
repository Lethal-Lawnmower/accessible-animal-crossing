/* hotkeys.h - Accessibility keyboard shortcuts.
 *
 * F5: repeat the last announcement. While the text editor is open, F5 reads
 *     back the whole text instead. (F5 was the previous project's repeat key.)
 *
 * Keys are read from the port's SDL event stream, so they work with the
 * game's own keyboard bindings untouched. */
#ifndef ACCESS_HOTKEYS_H
#define ACCESS_HOTKEYS_H

#ifdef __cplusplus
#include <SDL3/SDL_events.h>

/* Called for every SDL event the port receives. */
void acc_hotkeys_handle_event(const SDL_Event& event);
#endif

#endif /* ACCESS_HOTKEYS_H */
