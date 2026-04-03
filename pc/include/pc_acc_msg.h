/* pc_acc_msg.h - Dialogue TTS: speak message window text via screen reader */
#ifndef PC_ACC_MSG_H
#define PC_ACC_MSG_H

#include "m_msg.h"

/* Decode the current message buffer and speak it via TTS.
 * Call when a new message is loaded (mMsg_MainSetup_Appear, mMsg_ChangeMsgData). */
void pc_acc_msg_speak(mMsg_Window_c* msg_p);

/* Speak the next page of text starting from byte offset start_idx.
 * Call when the player presses A to advance past a BUTTON prompt. */
void pc_acc_msg_speak_from(mMsg_Window_c* msg_p, int start_idx);

/* Convert a game-encoded byte string to UTF-8. Returns number of bytes written
 * (not including NUL). Shared by dialogue and menu narration modules. */
int pc_acc_game_str_to_utf8(const u8* src, int src_len, char* dst, int dst_size);

#endif
