/* speech.h - Accessibility speech output through Prism.
 *
 * All project speech goes through this module. Prism picks the best
 * available output, normally the running screen reader. No other speech
 * system is used as a fallback.
 *
 * Text is UTF-8. Speaking never blocks the game: requests are handed to a
 * worker thread. An interrupting request discards anything still queued, so
 * a new focus or message replaces stale speech instead of piling up behind it.
 *
 * Safe to call before acc_speech_init() or when no output is available; the
 * request is then dropped (and logged once). */
#ifndef ACCESS_SPEECH_H
#define ACCESS_SPEECH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the speech worker. Call once at startup. */
void acc_speech_init(void);

/* Stop the worker and release Prism. Call once at exit. */
void acc_speech_shutdown(void);

/* True once Prism has found a usable output. */
bool acc_speech_available(void);

/* Speak text. interrupt = true stops current speech and drops queued
 * requests first; false queues after whatever is already playing. */
void acc_speak(const char* utf8, bool interrupt);

/* Convenience wrappers. */
void acc_speak_interrupt(const char* utf8);
void acc_speak_queue(const char* utf8);

/* Speak again the most recent interrupting announcement. */
void acc_speech_repeat(void);

/* Stop speech now and drop anything queued. */
void acc_speech_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_SPEECH_H */
