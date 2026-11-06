/* choice.h - Speak dialogue choice windows (Yes/No and other answer lists). */
#ifndef ACCESS_CHOICE_H
#define ACCESS_CHOICE_H

#include "m_choice.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The choice window became interactive. Queues the options after any
 * dialogue still being spoken, then the highlighted option. */
void acc_choice_opened(mChoice_c* choice);

/* Called every frame while the window is interactive; speaks the newly
 * highlighted option when the selection moves. */
void acc_choice_update(mChoice_c* choice);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_CHOICE_H */
