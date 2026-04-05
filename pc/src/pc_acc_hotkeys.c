/* pc_acc_hotkeys.c - Accessibility hotkeys for game state readouts.
 *
 * T = Time of day + weather
 * G = Bells (wallet)
 * M = Current location (scene/building name + acre)
 *
 * Also handles auto-announcements:
 * - Scene/building changes (entering/leaving buildings)
 * - Acre changes when walking outdoors
 *
 * Checked once per frame from the main game loop. */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_hotkeys.h"
#include "pc_acc_msg.h"
#include "m_common_data.h"
#include "m_private.h"
#include "m_kankyo.h"
#include "m_scene_table.h"
#include "m_field_info.h"
#include "m_player_lib.h"
#include "m_npc.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "m_mail.h"
#include "m_font.h"
#include "m_event.h"
#include "game.h"
#include "lb_rtc.h"

#include <SDL2/SDL.h>
#include <stdio.h>

/* Track previous key state to detect press (not hold) */
static u8 s_prev_t = 0;
static u8 s_prev_g = 0;
static u8 s_prev_m = 0;
static u8 s_prev_f5 = 0;
static u8 s_prev_f9 = 0;


/* Track scene changes for auto-announcement */
static int s_prev_scene = -1;

/* Track acre changes for outdoor announcements */
static int s_prev_acre_x = -1;
static int s_prev_acre_z = -1;

static const char* weather_name(int weather) {
    switch (weather) {
        case mEnv_WEATHER_CLEAR:  return "clear";
        case mEnv_WEATHER_RAIN:   return "raining";
        case mEnv_WEATHER_SNOW:   return "snowing";
        case mEnv_WEATHER_SAKURA: return "cherry blossoms";
        case mEnv_WEATHER_LEAVES: return "falling leaves";
        default:                  return "unknown";
    }
}

static const char* scene_name(int scene) {
    switch (scene) {
        case SCENE_FG:                    return "Outdoors";
        case SCENE_NPC_HOUSE:             return "Villager's House";
        case SCENE_SHOP0:                 return "Nook's Cranny";
        case SCENE_CONVENI:               return "Nook 'n' Go";
        case SCENE_SUPER:                 return "Nookway";
        case SCENE_DEPART:                return "Nookington's, first floor";
        case SCENE_DEPART_2:              return "Nookington's, second floor";
        case SCENE_POST_OFFICE:           return "Post Office";
        case SCENE_POLICE_BOX:            return "Police Station";
        case SCENE_MUSEUM_ENTRANCE:       return "Museum Entrance";
        case SCENE_MUSEUM_ROOM_PAINTING:  return "Museum, Paintings";
        case SCENE_MUSEUM_ROOM_FOSSIL:    return "Museum, Fossils";
        case SCENE_MUSEUM_ROOM_INSECT:    return "Museum, Insects";
        case SCENE_MUSEUM_ROOM_FISH:      return "Museum, Fish";
        case SCENE_NEEDLEWORK:            return "Able Sisters";
        case SCENE_MY_ROOM_S:
        case SCENE_MY_ROOM_M:
        case SCENE_MY_ROOM_L:
        case SCENE_MY_ROOM_LL1:
        case SCENE_MY_ROOM_LL2:           return "Your House";
        case SCENE_MY_ROOM_BASEMENT_S:
        case SCENE_MY_ROOM_BASEMENT_M:
        case SCENE_MY_ROOM_BASEMENT_L:
        case SCENE_MY_ROOM_BASEMENT_LL1:  return "Your Basement";
        case SCENE_COTTAGE_MY:            return "Island House";
        case SCENE_COTTAGE_NPC:           return "Island Villager House";
        case SCENE_BROKER_SHOP:           return "Crazy Redd's Tent";
        case SCENE_LIGHTHOUSE:            return "Lighthouse";
        case SCENE_TENT:                  return "Tent";
        case SCENE_PLAYERSELECT:
        case SCENE_PLAYERSELECT_2:
        case SCENE_PLAYERSELECT_3:        return "Player Select";
        case SCENE_TITLE_DEMO:            return "Title Screen";
        case SCENE_EVENT_ANNOUNCEMENT:    return "Event Plaza";
        case SCENE_START_DEMO:
        case SCENE_START_DEMO2:
        case SCENE_START_DEMO3:           return "Train";
        default:                          return "Unknown Location";
    }
}

/* Convert block coordinates to a human-readable acre label.
 * The playable outdoor area is a 5x6 grid (X=1..5, Z=1..6).
 * Rows are labeled A-F (top to bottom, Z axis), columns 1-5 (left to right, X axis).
 * Example: block (1,1) = "A-1", block (3,4) = "D-3" */
static const char* acre_label(int bx, int bz) {
    /* Only label playable FG area (X: 1-5, Z: 1-6) */
    if (bx < 1 || bx > FG_BLOCK_X_NUM || bz < 1 || bz > FG_BLOCK_Z_NUM) {
        return NULL;
    }
    static char label[8];
    snprintf(label, sizeof(label), "%c-%d", 'A' + (bz - 1), bx);
    return label;
}

void pc_acc_hotkeys_update(void) {
    if (!pc_acc_is_active()) return;

    int scene = Save_Get(scene_no);

    /* Auto-announce scene/building changes */
    if (scene != s_prev_scene && s_prev_scene != -1) {
        if (scene == SCENE_NPC_HOUSE) {
            /* Try to get the villager's name for their house */
            mActor_name_t house_id = Common_Get(house_owner_name);
            if (ITEM_IS_NPC_HOUSE(house_id)) {
                mActor_name_t npc_id = NPC_HOUSE_ID_TO_NPC_ID(house_id);
                u8 npc_name[ANIMAL_NAME_LEN];
                mNpc_GetNpcWorldNameTableNo(npc_name, npc_id);
                int len = mMl_strlen(npc_name, ANIMAL_NAME_LEN, CHAR_SPACE);
                if (len > 0) {
                    char name_buf[32];
                    int wrote = pc_acc_game_str_to_utf8(npc_name, len, name_buf, sizeof(name_buf));
                    if (wrote > 0) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%s's House", name_buf);
                        pc_acc_speak_interrupt(buf);
                    } else {
                        pc_acc_speak_interrupt("Villager's House");
                    }
                } else {
                    pc_acc_speak_interrupt("Villager's House");
                }
            } else {
                pc_acc_speak_interrupt("Villager's House");
            }
        } else {
            const char* name = scene_name(scene);
            if (name) {
                pc_acc_speak_interrupt(name);
            }
        }
        /* Reset acre tracking when entering/leaving outdoors */
        s_prev_acre_x = -1;
        s_prev_acre_z = -1;
    }
    s_prev_scene = scene;

    /* Auto-announce acre changes when outdoors */
    if (scene == SCENE_FG && gamePT != NULL) {
        ACTOR* player = GET_PLAYER_ACTOR_GAME_ACTOR(gamePT);
        if (player != NULL) {
            int bx, bz;
            mFI_Wpos2BlockNum(&bx, &bz, player->world.position);
            if ((bx != s_prev_acre_x || bz != s_prev_acre_z) && s_prev_acre_x != -1) {
                const char* label = acre_label(bx, bz);
                if (label) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Acre %s", label);
                    pc_acc_speak_interrupt(buf);
                }
            }
            s_prev_acre_x = bx;
            s_prev_acre_z = bz;
        }
    }

    const u8* keys = SDL_GetKeyboardState(NULL);
    if (!keys) return;

    u8 t_key = keys[SDL_SCANCODE_T];
    u8 g_key = keys[SDL_SCANCODE_G];
    u8 m_key = keys[SDL_SCANCODE_M];
    u8 f5_key = keys[SDL_SCANCODE_F5];

    /* F5: Repeat last spoken text */
    if (f5_key && !s_prev_f5) {
        pc_acc_repeat_last();
    }
    s_prev_f5 = f5_key;

    /* T: Time + Weather */
    if (t_key && !s_prev_t) {
        lbRTC_time_c t = Common_Get(time.rtc_time);
        s16 w = Common_Get(weather);
        int hour12 = t.hour % 12;
        if (hour12 == 0) hour12 = 12;
        const char* ampm = (t.hour < 12) ? "a.m." : "p.m.";

        char buf[128];
        snprintf(buf, sizeof(buf), "%d:%02d %s, %s",
                 hour12, t.min, ampm, weather_name(w));
        pc_acc_speak_interrupt(buf);
    }

    /* G: Bells */
    if (g_key && !s_prev_g) {
        Private_c* priv = Common_Get(now_private);
        char buf[64];
        if (priv) {
            snprintf(buf, sizeof(buf), "%u bells", (unsigned)priv->inventory.wallet);
        } else {
            snprintf(buf, sizeof(buf), "No wallet data");
        }
        pc_acc_speak_interrupt(buf);
    }

    /* M: Location (scene + acre if outdoors, NPC name if in their house) */
    if (m_key && !s_prev_m) {
        if (scene == SCENE_FG && gamePT != NULL) {
            ACTOR* player = GET_PLAYER_ACTOR_GAME_ACTOR(gamePT);
            if (player != NULL) {
                int bx, bz;
                mFI_Wpos2BlockNum(&bx, &bz, player->world.position);
                const char* label = acre_label(bx, bz);
                if (label) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Outdoors, Acre %s", label);
                    pc_acc_speak_interrupt(buf);
                } else {
                    pc_acc_speak_interrupt("Outdoors");
                }
            } else {
                pc_acc_speak_interrupt("Outdoors");
            }
        } else if (scene == SCENE_NPC_HOUSE) {
            mActor_name_t house_id = Common_Get(house_owner_name);
            if (ITEM_IS_NPC_HOUSE(house_id)) {
                mActor_name_t npc_id = NPC_HOUSE_ID_TO_NPC_ID(house_id);
                u8 npc_name[ANIMAL_NAME_LEN];
                mNpc_GetNpcWorldNameTableNo(npc_name, npc_id);
                int len = mMl_strlen(npc_name, ANIMAL_NAME_LEN, CHAR_SPACE);
                if (len > 0) {
                    char name_buf[32];
                    int wrote = pc_acc_game_str_to_utf8(npc_name, len, name_buf, sizeof(name_buf));
                    if (wrote > 0) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%s's House", name_buf);
                        pc_acc_speak_interrupt(buf);
                    } else {
                        pc_acc_speak_interrupt("Villager's House");
                    }
                } else {
                    pc_acc_speak_interrupt("Villager's House");
                }
            } else {
                pc_acc_speak_interrupt("Villager's House");
            }
        } else {
            pc_acc_speak_interrupt(scene_name(scene));
        }
    }

    /* F9: Skip prologue (debug — completes all intro/tutorial flags) */
    u8 f9_key = keys[SDL_SCANCODE_F9];
    if (f9_key && !s_prev_f9) {
        if (mEv_CheckFirstIntro() || mEv_CheckFirstJob()) {
            /* Clear all prologue event flags for the current player */
            int player_no = Common_Get(player_no);
            mEv_ClearPersonalEventFlag(player_no);
            pc_acc_speak_interrupt("Prologue skipped");
        } else {
            pc_acc_speak_interrupt("Not in prologue");
        }
    }
    s_prev_f9 = f9_key;

    s_prev_t = t_key;
    s_prev_g = g_key;
    s_prev_m = m_key;
}

#else
/* Non-Windows stub */
void pc_acc_hotkeys_update(void) {}
#endif
