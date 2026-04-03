/* pc_acc_magnet.c - Magnet pickup system for accessibility.
 *
 * Auto-collects items that the player deliberately generated
 * (tree shakes, rock hits, etc.) without requiring precise navigation.
 * Items dropped from the player's inventory are NOT collected.
 *
 * The drop system calls pc_acc_magnet_notify_drop() when an item lands
 * on the FG grid. We track those positions and auto-collect them each
 * frame if the player has inventory space.
 *
 * Toggle: F1 key
 * Announces each collected item via TTS. */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_magnet.h"
#include "pc_acc_msg.h"
#include "m_actor.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "m_field_info.h"
#include "m_private.h"
#include "m_common_data.h"
#include "m_font.h"
#include "game.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========================================================================= */
/* Configuration                                                             */
/* ========================================================================= */

#define MAGNET_MAX_TRACKED 32
#define MAGNET_PICKUP_DELAY 30   /* frames to wait after drop before collecting */
#define MAGNET_EXPIRE_TIME  300  /* frames (~10 sec) before we stop tracking a drop */
#define MAGNET_RADIUS       200.0f /* world units — pickup range from player */

/* ========================================================================= */
/* State                                                                     */
/* ========================================================================= */

typedef struct {
    mActor_name_t item;
    xyz_t pos;           /* world position where item landed */
    int age;             /* frames since item landed */
    int active;          /* 1 = being tracked, 0 = slot free */
} MagnetEntry;

static MagnetEntry s_tracked[MAGNET_MAX_TRACKED];
static int s_enabled = 1;        /* magnet toggle — on by default */
static u8 s_prev_f1 = 0;

/* ========================================================================= */
/* Drop notification — called from bIT_actor_drop_move_fly_destruct          */
/* ========================================================================= */

void pc_acc_magnet_notify_drop(mActor_name_t item, f32 x, f32 y, f32 z) {
    if (!s_enabled) return;
    if (item == EMPTY_NO || item == RSV_NO) return;

    /* Don't track weeds, holes, or other non-collectible ground items */
    if (IS_ITEM_GRASS(item)) return;
    if (item >= HOLE_START && item <= HOLE_END) return;

    /* Find a free slot */
    for (int i = 0; i < MAGNET_MAX_TRACKED; i++) {
        if (!s_tracked[i].active) {
            s_tracked[i].item = item;
            s_tracked[i].pos.x = x;
            s_tracked[i].pos.y = y;
            s_tracked[i].pos.z = z;
            s_tracked[i].age = 0;
            s_tracked[i].active = 1;
            return;
        }
    }

    /* Table full — overwrite oldest entry */
    int oldest_idx = 0;
    int oldest_age = 0;
    for (int i = 0; i < MAGNET_MAX_TRACKED; i++) {
        if (s_tracked[i].age > oldest_age) {
            oldest_age = s_tracked[i].age;
            oldest_idx = i;
        }
    }
    s_tracked[oldest_idx].item = item;
    s_tracked[oldest_idx].pos.x = x;
    s_tracked[oldest_idx].pos.y = y;
    s_tracked[oldest_idx].pos.z = z;
    s_tracked[oldest_idx].age = 0;
    s_tracked[oldest_idx].active = 1;
}

/* ========================================================================= */
/* Try to collect a single tracked item                                      */
/* ========================================================================= */

static int magnet_try_collect(MagnetEntry* entry) {
    /* Verify the item is still on the FG grid at this position */
    mActor_name_t* fg_p = mFI_GetUnitFG(entry->pos);
    if (!fg_p) return 0;
    if (*fg_p != entry->item) {
        /* Item was already picked up or changed — stop tracking */
        entry->active = 0;
        return 0;
    }

    /* Find an empty inventory slot */
    int slot = mPlib_Get_space_putin_item();
    if (slot < 0) return 0; /* inventory full */

    /* Add to player's inventory */
    mPr_SetPossessionItem(Now_Private, slot, entry->item, mPr_ITEM_COND_NORMAL);

    /* Remove from FG grid */
    mFI_SetFG_common(EMPTY_NO, entry->pos, TRUE);

    /* Mark as collected */
    entry->active = 0;

    return 1;
}

/* ========================================================================= */
/* Get item name for TTS                                                     */
/* ========================================================================= */

static void magnet_get_item_name(mActor_name_t item, char* buf, int buf_size) {
    buf[0] = '\0';

    int type = ITEM_NAME_GET_TYPE(item);
    if (type <= NAME_TYPE_FTR1) {
        u8 item_name[mIN_ITEM_NAME_LEN];
        mIN_copy_name_str(item_name, item);
        int len = mMl_strlen(item_name, mIN_ITEM_NAME_LEN, CHAR_SPACE);
        if (len > 0) {
            pc_acc_game_str_to_utf8(item_name, len, buf, buf_size);
        }
    }

    /* Fallback for money bags */
    if (buf[0] == '\0') {
        if (item >= ITM_MONEY_START && item <= ITM_MONEY_END) {
            switch (item) {
                case ITM_MONEY_100:   snprintf(buf, buf_size, "100 Bells"); break;
                case ITM_MONEY_1000:  snprintf(buf, buf_size, "1000 Bells"); break;
                case ITM_MONEY_10000: snprintf(buf, buf_size, "10000 Bells"); break;
                case ITM_MONEY_30000: snprintf(buf, buf_size, "30000 Bells"); break;
                default:              snprintf(buf, buf_size, "Bells"); break;
            }
        }
    }
}

/* ========================================================================= */
/* Main update — called once per frame                                       */
/* ========================================================================= */

void pc_acc_magnet_update(void) {
    if (!pc_acc_is_active()) return;

    /* F1 toggle */
    const u8* keys = SDL_GetKeyboardState(NULL);
    if (keys) {
        u8 f1 = keys[SDL_SCANCODE_F1];
        if (f1 && !s_prev_f1) {
            s_enabled = !s_enabled;
            if (s_enabled) {
                pc_acc_speak_interrupt("Magnet pickup on");
            } else {
                pc_acc_speak_interrupt("Magnet pickup off");
                /* Clear all tracked items when disabled */
                for (int i = 0; i < MAGNET_MAX_TRACKED; i++) {
                    s_tracked[i].active = 0;
                }
            }
        }
        s_prev_f1 = f1;
    }

    if (!s_enabled) return;

    /* Need player position for range check */
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    /* Only work outdoors (SCENE_FG) — drops only happen outdoors */
    int scene = Save_Get(scene_no);
    if (scene != SCENE_FG) return;

    xyz_t player_pos = player->actor_class.world.position;

    /* Age all entries, expire old ones, and try to collect ready items */
    for (int i = 0; i < MAGNET_MAX_TRACKED; i++) {
        if (!s_tracked[i].active) continue;

        s_tracked[i].age++;

        /* Expire entries that are too old */
        if (s_tracked[i].age > MAGNET_EXPIRE_TIME) {
            s_tracked[i].active = 0;
            continue;
        }

        /* Wait for drop animation to finish before collecting */
        if (s_tracked[i].age < MAGNET_PICKUP_DELAY) continue;

        /* Range check */
        f32 dx = s_tracked[i].pos.x - player_pos.x;
        f32 dz = s_tracked[i].pos.z - player_pos.z;
        f32 dist_sq = dx * dx + dz * dz;
        if (dist_sq > MAGNET_RADIUS * MAGNET_RADIUS) continue;

        /* Try to collect */
        mActor_name_t item = s_tracked[i].item;
        if (magnet_try_collect(&s_tracked[i])) {
            /* Announce the collected item */
            char name[32];
            magnet_get_item_name(item, name, sizeof(name));
            if (name[0] != '\0') {
                char buf[64];
                snprintf(buf, sizeof(buf), "Picked up %s", name);
                pc_acc_speak_queue(buf);
            }
        }
    }
}

#else
/* Non-Windows stubs */
void pc_acc_magnet_update(void) {}
void pc_acc_magnet_notify_drop(mActor_name_t item, f32 x, f32 y, f32 z) {
    (void)item; (void)x; (void)y; (void)z;
}
#endif
