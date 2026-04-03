/* pc_accessibility.h - Accessibility layer: TTS via Tolk, announce helpers */
#ifndef PC_ACCESSIBILITY_H
#define PC_ACCESSIBILITY_H

#include <stdbool.h>

/* Initialize accessibility system (loads Tolk.dll if present). Call once at startup. */
void pc_acc_init(void);

/* Shut down accessibility system. Call at exit. */
void pc_acc_shutdown(void);

/* Returns true if a screen reader was detected and is active. */
bool pc_acc_is_active(void);

/* Speak text aloud. If interrupt is true, stop current speech first.
 * str is UTF-8; converted to wchar_t internally for Tolk. */
void pc_acc_speak(const char* str, bool interrupt);

/* Speak text without interrupting current speech. */
void pc_acc_speak_queue(const char* str);

/* Speak text, interrupting any current speech. */
void pc_acc_speak_interrupt(const char* str);

/* Output to braille display only (no speech). str is UTF-8. */
void pc_acc_braille(const char* str);

/* Speak + braille combined. */
void pc_acc_output(const char* str, bool interrupt);

/* Repeat the last spoken text. */
void pc_acc_repeat_last(void);

/* Stop any current speech immediately. */
void pc_acc_silence(void);

/* Returns true if the screen reader is currently speaking. */
bool pc_acc_is_speaking(void);

#endif /* PC_ACCESSIBILITY_H */
