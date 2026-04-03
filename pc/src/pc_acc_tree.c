/* pc_acc_tree.c - Tree interaction and balloon present accessibility.
 *
 * TREES:
 *   Proximity: announces tree type + contents when player is adjacent.
 *     "Apple tree, 3 fruit" / "Oak tree, wasps" / "Cedar tree, empty"
 *   Shake: immediate wasp warning, drop result TTS, empty notification.
 *   Stump detection included.
 *
 * BALLOONS:
 *   Spawn → "Balloon spotted, traveling [direction]"
 *   Nearby → "Balloon nearby" (within 1-acre distance)
 *   Stuck → "Balloon stuck in tree, [direction], shake tree to get it"
 *   Escaping soon → "Balloon escaping soon, hurry" (30s warning)
 *   Present dropped → "Present dropped, [direction], [steps] steps"
 *   Lost → "Present fell in water, lost"
 *
 * Checked once per frame from the main game loop. */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_tree.h"
#include "pc_acc_msg.h"
#include "m_common_data.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "m_field_info.h"
#include "m_scene_table.h"
#include "m_player_lib.h"
#include "m_play.h"
#include "m_actor.h"
#include "m_font.h"
#include "m_kankyo.h"
#include "m_fuusen.h"
#include "ac_fuusen.h"
#include "m_collision_bg.h"
#include "game.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* ========================================================================= */
/* Compass helper (4 cardinal only)                                          */
/* ========================================================================= */

static const char* tree_compass(f32 dx, f32 dz) {
    f32 ax = dx > 0 ? dx : -dx;
    f32 az = dz > 0 ? dz : -dz;
    if (az >= ax) return (dz < 0) ? "north" : "south";
    else return (dx > 0) ? "east" : "west";
}

static int tree_steps(f32 dist) {
    int s = (int)(dist / 20.0f + 0.5f);
    if (s < 1 && dist > 1.0f) s = 1;
    return s;
}

/* ========================================================================= */
/* Tree proximity announcements                                              */
/* ========================================================================= */

/* Track last announced tree tile to avoid repeating */
static int s_last_tree_ux = -1;
static int s_last_tree_uz = -1;

/* Describe a tree item for TTS */
static const char* tree_type_name(mActor_name_t item) {
    /* Fruit trees with fruit */
    if (item == TREE_APPLE_FRUIT) return "Apple tree, 3 fruit";
    if (item == TREE_ORANGE_FRUIT) return "Orange tree, 3 fruit";
    if (item == TREE_PEACH_FRUIT) return "Peach tree, 3 fruit";
    if (item == TREE_PEAR_FRUIT) return "Pear tree, 3 fruit";
    if (item == TREE_CHERRY_FRUIT) return "Cherry tree, 3 fruit";
    if (item == TREE_PALM_FRUIT) return "Coconut tree, 2 fruit";

    /* Fruit trees harvested */
    if (item == TREE_APPLE_NOFRUIT_0 || item == TREE_APPLE_NOFRUIT_1 || item == TREE_APPLE_NOFRUIT_2)
        return "Apple tree, empty";
    if (item == TREE_ORANGE_NOFRUIT_0 || item == TREE_ORANGE_NOFRUIT_1 || item == TREE_ORANGE_NOFRUIT_2)
        return "Orange tree, empty";
    if (item == TREE_PEACH_NOFRUIT_0 || item == TREE_PEACH_NOFRUIT_1 || item == TREE_PEACH_NOFRUIT_2)
        return "Peach tree, empty";
    if (item == TREE_PEAR_NOFRUIT_0 || item == TREE_PEAR_NOFRUIT_1 || item == TREE_PEAR_NOFRUIT_2)
        return "Pear tree, empty";
    if (item == TREE_CHERRY_NOFRUIT_0 || item == TREE_CHERRY_NOFRUIT_1 || item == TREE_CHERRY_NOFRUIT_2)
        return "Cherry tree, empty";
    if (item == TREE_PALM_NOFRUIT_0 || item == TREE_PALM_NOFRUIT_1 || item == TREE_PALM_NOFRUIT_2)
        return "Coconut tree, empty";

    /* Money trees */
    if (item == TREE_100BELLS) return "Money tree, 100 bells";
    if (item == TREE_1000BELLS) return "Money tree, 1000 bells";
    if (item == TREE_10000BELLS) return "Money tree, 10000 bells";
    if (item == TREE_30000BELLS) return "Money tree, 30000 bells";

    /* Special trees */
    if (item == TREE_BELLS) return "Oak tree, bells hidden";
    if (item == CEDAR_TREE_BELLS) return "Cedar tree, bells hidden";
    if (item == GOLD_TREE_BELLS) return "Gold tree, bells hidden";

    if (item == TREE_FTR) return "Oak tree, furniture hidden";
    if (item == CEDAR_TREE_FTR) return "Cedar tree, furniture hidden";
    if (item == GOLD_TREE_FTR) return "Gold tree, furniture hidden";

    if (item == TREE_BEES) return "Oak tree, wasps";
    if (item == CEDAR_TREE_BEES) return "Cedar tree, wasps";
    if (item == GOLD_TREE_BEES) return "Gold tree, wasps";

    if (item == TREE_PRESENT) return "Oak tree, present";
    if (item == TREE_LIGHTS) return "Oak tree, Christmas lights";
    if (item == CEDAR_TREE_LIGHTS) return "Cedar tree, Christmas lights";

    /* Gold tree */
    if (item == GOLD_TREE) return "Gold tree";
    if (item == GOLD_TREE_SHOVEL) return "Gold tree, golden shovel";

    /* Plain trees */
    if (item == TREE) return "Oak tree";
    if (item == CEDAR_TREE) return "Cedar tree";

    /* Stumps */
    if (IS_ITEM_TREE_STUMP(item)) return "Stump";

    return NULL;
}

/* Check player proximity to trees — called every frame */
static void tree_proximity_check(void) {
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    int scene = Save_Get(scene_no);
    if (scene != SCENE_FG) {
        s_last_tree_ux = -1;
        s_last_tree_uz = -1;
        return;
    }

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t pos = player->actor_class.world.position;
    int pux, puz;
    if (!mFI_Wpos2UtNum(&pux, &puz, pos)) return;

    /* Scan the 8 adjacent tiles + current tile for trees */
    static const int ddx[] = { 0, 1, 1, 1, 0, -1, -1, -1, 0 };
    static const int ddz[] = { -1, -1, 0, 1, 1, 1, 0, -1, 0 };

    for (int i = 0; i < 9; i++) {
        int ux = pux + ddx[i];
        int uz = puz + ddz[i];

        /* Convert unit coords to world position and read FG item */
        xyz_t wpos;
        mFI_UtNum2CenterWpos(&wpos, ux, uz);
        mActor_name_t* fg_p = mFI_GetUnitFG(wpos);
        if (!fg_p) continue;

        mActor_name_t item = *fg_p;
        if (item == EMPTY_NO) continue;

        const char* desc = tree_type_name(item);
        if (!desc) continue;

        /* Found a tree — announce if it's a new one */
        if (ux == s_last_tree_ux && uz == s_last_tree_uz) return; /* same tree, don't repeat */

        s_last_tree_ux = ux;
        s_last_tree_uz = uz;
        pc_acc_speak_queue(desc);
        return;
    }

    /* No tree adjacent — clear tracking so re-entering range re-announces */
    s_last_tree_ux = -1;
    s_last_tree_uz = -1;
}

/* ========================================================================= */
/* Tree shake hooks                                                          */
/* ========================================================================= */

static mActor_name_t s_last_shaken_item = EMPTY_NO;

void pc_acc_tree_shaken(mActor_name_t item, int ut_x, int ut_z) {
    if (!pc_acc_is_active()) return;

    s_last_shaken_item = item;

    /* Immediate wasp warning — only 5 frames before they spawn! */
    if (item == TREE_BEES || item == CEDAR_TREE_BEES || item == GOLD_TREE_BEES) {
        pc_acc_speak_interrupt("Wasps incoming, run!");
    }
}

void pc_acc_tree_drop(mActor_name_t dropped_item, int count) {
    if (!pc_acc_is_active()) return;

    /* Don't announce for bee trees — already said "Wasps incoming" */
    if (s_last_shaken_item == TREE_BEES || s_last_shaken_item == CEDAR_TREE_BEES ||
        s_last_shaken_item == GOLD_TREE_BEES) {
        /* But do announce the honeycomb that dropped */
        if (dropped_item == HONEYCOMB) {
            pc_acc_speak_queue("Honeycomb fell");
        }
        return;
    }

    /* Resolve item name */
    char buf[80];

    /* Money items */
    if (dropped_item == ITM_MONEY_100) {
        snprintf(buf, sizeof(buf), "100 bells fell");
    } else if (dropped_item == ITM_MONEY_1000) {
        snprintf(buf, sizeof(buf), "1000 bells fell");
    } else if (dropped_item == ITM_MONEY_10000) {
        snprintf(buf, sizeof(buf), "10000 bells fell");
    } else if (dropped_item == ITM_MONEY_30000) {
        snprintf(buf, sizeof(buf), "30000 bells fell");
    } else if (dropped_item == ITM_PRESENT) {
        snprintf(buf, sizeof(buf), "Present fell");
    } else if (dropped_item == ITM_GOLDEN_SHOVEL) {
        snprintf(buf, sizeof(buf), "Golden shovel fell");
    } else {
        /* Use game's item name system */
        u8 item_name[mIN_ITEM_NAME_LEN];
        mIN_copy_name_str(item_name, dropped_item);
        int len = mMl_strlen(item_name, mIN_ITEM_NAME_LEN, CHAR_SPACE);
        if (len > 0) {
            char name_utf8[48];
            pc_acc_game_str_to_utf8(item_name, len, name_utf8, sizeof(name_utf8));
            snprintf(buf, sizeof(buf), "%s fell", name_utf8);
        } else {
            snprintf(buf, sizeof(buf), "Item fell");
        }
    }

    pc_acc_speak_interrupt(buf);
}

/* ========================================================================= */
/* Balloon tracking                                                          */
/* ========================================================================= */

/* Balloon state tracking for per-frame updates */
static int s_balloon_warned_escape = 0;
static int s_balloon_announced_nearby = 0;

void pc_acc_balloon_spawned(void) {
    if (!pc_acc_is_active()) return;

    s_balloon_warned_escape = 0;
    s_balloon_announced_nearby = 0;

    /* Determine travel direction from wind */
    s16 wind_angle = mEnv_GetWindAngleS();
    /* Convert s16 angle to rough cardinal: 0=south, +/-0x4000=east/west, 0x8000=north
     * In AC angle system: 0 = south (+Z), 0x4000 = west (-X), 0x8000 = north (-Z), 0xC000 = east (+X) */
    f32 dx = sin_s(wind_angle);
    f32 dz = cos_s(wind_angle);
    const char* dir = tree_compass(dx, dz);

    char buf[64];
    snprintf(buf, sizeof(buf), "Balloon spotted, traveling %s", dir);
    pc_acc_speak_queue(buf);
}

void pc_acc_balloon_stuck(f32 bx, f32 bz) {
    if (!pc_acc_is_active()) return;

    s_balloon_warned_escape = 0;

    /* Get direction from player to balloon */
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;
    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t ppos = player->actor_class.world.position;
    f32 dx = bx - ppos.x;
    f32 dz = bz - ppos.z;
    f32 dist = sqrtf(dx * dx + dz * dz);
    const char* dir = tree_compass(dx, dz);
    int steps = tree_steps(dist);

    char buf[96];
    snprintf(buf, sizeof(buf), "Balloon stuck in tree, %s, %d steps, shake tree to get it", dir, steps);
    pc_acc_speak_interrupt(buf);
}

void pc_acc_balloon_escaping(int from_tree, f32 bx, f32 bz) {
    if (!pc_acc_is_active()) return;

    if (from_tree) {
        pc_acc_speak_interrupt("Balloon escaping");
    }
}

void pc_acc_balloon_present_dropped(f32 px, f32 pz) {
    if (!pc_acc_is_active()) return;

    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;
    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t ppos = player->actor_class.world.position;
    f32 dx = px - ppos.x;
    f32 dz = pz - ppos.z;
    f32 dist = sqrtf(dx * dx + dz * dz);
    const char* dir = tree_compass(dx, dz);
    int steps = tree_steps(dist);

    /* Check if the drop tile is walkable (not water) */
    int ut_x, ut_z;
    xyz_t drop_pos = { px, 0.0f, pz };
    if (mFI_Wpos2UtNum(&ut_x, &ut_z, drop_pos)) {
        if (!mCoBG_Unit2CheckNpc(ut_x, ut_z)) {
            pc_acc_speak_interrupt("Present fell in water, lost");
            return;
        }
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "Present dropped, %s, %d steps", dir, steps);
    pc_acc_speak_interrupt(buf);
}

/* Per-frame balloon monitoring — escape timer warning + nearby detection */
static void balloon_frame_check(void) {
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    int scene = Save_Get(scene_no);
    if (scene != SCENE_FG) return;

    /* Find the balloon actor by scanning the control actor list */
    Actor_info* ai = &play->actor_info;
    ACTOR* actor = ai->list[ACTOR_PART_CONTROL].actor;
    FUUSEN_ACTOR* fuusen = NULL;

    while (actor != NULL) {
        if (actor->id == mAc_PROFILE_FUUSEN) {
            fuusen = (FUUSEN_ACTOR*)actor;
            break;
        }
        actor = actor->next_actor;
    }

    if (!fuusen) {
        s_balloon_announced_nearby = 0;
        s_balloon_warned_escape = 0;
        return;
    }

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t ppos = player->actor_class.world.position;
    f32 dx = fuusen->actor_class.world.position.x - ppos.x;
    f32 dz = fuusen->actor_class.world.position.z - ppos.z;
    f32 dist = sqrtf(dx * dx + dz * dz);

    /* "Balloon nearby" when within ~1 acre (640 units) while moving */
    if (fuusen->action == aFSN_ACTION_MOVING && !s_balloon_announced_nearby) {
        if (dist < 640.0f) {
            const char* dir = tree_compass(dx, dz);
            char buf[64];
            snprintf(buf, sizeof(buf), "Balloon nearby, %s", dir);
            pc_acc_speak_queue(buf);
            s_balloon_announced_nearby = 1;
        }
    }

    /* Escape timer warning while stuck on tree (~30 seconds = 900 frames before escape) */
    if (fuusen->action == aFSN_ACTION_WOOD_STOP && !s_balloon_warned_escape) {
        if (fuusen->escape_timer <= aFSN_ESCAPE_TIMER + 900 &&
            fuusen->escape_timer > aFSN_ESCAPE_TIMER) {
            pc_acc_speak_interrupt("Balloon escaping soon, hurry");
            s_balloon_warned_escape = 1;
        }
    }
}

/* ========================================================================= */
/* Main per-frame update                                                     */
/* ========================================================================= */

void pc_acc_tree_update(void) {
    if (!pc_acc_is_active()) return;

    tree_proximity_check();
    balloon_frame_check();
}

#else
/* Non-Windows stubs */
void pc_acc_tree_update(void) {}
void pc_acc_tree_shaken(mActor_name_t item, int ut_x, int ut_z) {}
void pc_acc_tree_drop(mActor_name_t dropped_item, int count) {}
void pc_acc_balloon_spawned(void) {}
void pc_acc_balloon_stuck(f32 bx, f32 bz) {}
void pc_acc_balloon_escaping(int from_tree, f32 bx, f32 bz) {}
void pc_acc_balloon_present_dropped(f32 px, f32 pz) {}
#endif
