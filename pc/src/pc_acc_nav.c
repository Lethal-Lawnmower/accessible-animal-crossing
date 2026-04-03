/* pc_acc_nav.c - Navigation assistance for blind players.
 *
 * Category-based scanning: NPCs, Buildings, Items
 * Keybinds (matching Pokémon Access / LADX conventions):
 *   Shift+L = next category    Shift+J = prev category
 *   L = next item in category  J = prev item
 *   K = directions to selected target
 *   Shift+K = A* pathfinding with turn-by-turn directions
 *
 * Auto-announcements: NPC proximity alerts when entering range.
 * Checked once per frame from the main game loop. */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_nav.h"
#include "pc_acc_msg.h"
#include "m_actor.h"
#include "m_play.h"
#include "m_player.h"
#include "m_player_lib.h"
#include "m_npc.h"
#include "ac_npc.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "m_scene_table.h"
#include "m_field_info.h"
#include "m_collision_bg.h"
#include "m_common_data.h"
#include "m_font.h"
#include "m_house.h"
#include "m_private.h"
#include "m_random_field.h"
#include "m_personal_id.h"
#include "game.h"

#include "pc_keybindings.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

/* ========================================================================= */
/* Nav categories                                                            */
/* ========================================================================= */

typedef enum {
    NAV_CAT_VILLAGERS,
    NAV_CAT_BUILDINGS,
    NAV_CAT_HOUSES,
    NAV_CAT_ITEMS,
    NAV_CAT_COUNT,
    NAV_CAT_TERRAIN = 99 /* internal only, not in cycle */
} NavCategory;

static const char* s_cat_names[] = { "Villagers", "Buildings", "Houses", "Items" };

/* ========================================================================= */
/* Scan results                                                              */
/* ========================================================================= */

#define NAV_MAX_RESULTS 64
#define NAV_SCAN_RADIUS 2000.0f
#define NAV_PROXIMITY_RADIUS 200.0f

typedef struct {
    ACTOR* actor;     /* NULL for FG grid items (shop items, room furniture) */
    f32 dist;
    f32 dx;           /* world x delta (east/west) */
    f32 dz;           /* world z delta (north/south) */
    xyz_t world_pos;  /* cached world position (used when actor is NULL) */
    mActor_name_t item_id; /* FG item id (when actor is NULL) */
    char name[64];
    u8 in_current_acre; /* 1 if entity is in the player's current acre */
} NavResult;

static NavCategory s_category = NAV_CAT_VILLAGERS;
static NavResult s_results[NAV_MAX_RESULTS];
static int s_result_count = 0;
static int s_nav_index = -1;

/* Target lock: persist selection across refreshes by matching name + npc_id */
static char s_locked_name[64] = "";
static mActor_name_t s_locked_npc_id = 0;
static xyz_t s_locked_pos = {0, 0, 0};
static int s_has_lock = 0;

/* Previous key states for edge detection */
static u8 s_prev_l = 0;
static u8 s_prev_j = 0;
static u8 s_prev_k = 0;

/* NPC proximity tracking — remember which NPCs we've already announced */
#define MAX_PROXIMITY_TRACKED 16
static mActor_name_t s_proximity_announced[MAX_PROXIMITY_TRACKED];
static int s_proximity_count = 0;
static int s_prev_scene_nav = -1;

/* Tile tick sound — generated WAV in memory, played via PlaySound */
#define TICK_SAMPLE_RATE 8000
#define TICK_DURATION_MS 60
#define TICK_NUM_SAMPLES (TICK_SAMPLE_RATE * TICK_DURATION_MS / 1000)
#define TICK_WAV_SIZE (44 + TICK_NUM_SAMPLES * 2)

static BYTE s_tick_wav[TICK_WAV_SIZE];
static int s_tick_ready = 0;
static int s_prev_tile_x = -1;
static int s_prev_tile_z = -1;

static void nav_generate_tick_wav(void) {
    int data_size = TICK_NUM_SAMPLES * 2; /* 16-bit mono samples */
    int file_size = 36 + data_size;
    BYTE* p = s_tick_wav;

    /* RIFF header */
    memcpy(p, "RIFF", 4); p += 4;
    p[0] = file_size & 0xFF; p[1] = (file_size >> 8) & 0xFF;
    p[2] = (file_size >> 16) & 0xFF; p[3] = (file_size >> 24) & 0xFF; p += 4;
    memcpy(p, "WAVE", 4); p += 4;

    /* fmt chunk */
    memcpy(p, "fmt ", 4); p += 4;
    p[0] = 16; p[1] = 0; p[2] = 0; p[3] = 0; p += 4; /* chunk size = 16 */
    p[0] = 1; p[1] = 0; p += 2; /* PCM format */
    p[0] = 1; p[1] = 0; p += 2; /* mono */
    int sr = TICK_SAMPLE_RATE;
    p[0] = sr & 0xFF; p[1] = (sr >> 8) & 0xFF; p[2] = 0; p[3] = 0; p += 4;
    int bps = sr * 2; /* bytes per second (mono 16-bit) */
    p[0] = bps & 0xFF; p[1] = (bps >> 8) & 0xFF; p[2] = (bps >> 16) & 0xFF; p[3] = 0; p += 4;
    p[0] = 2; p[1] = 0; p += 2; /* block align = 2 */
    p[0] = 16; p[1] = 0; p += 2; /* bits per sample */

    /* data chunk */
    memcpy(p, "data", 4); p += 4;
    p[0] = data_size & 0xFF; p[1] = (data_size >> 8) & 0xFF;
    p[2] = (data_size >> 16) & 0xFF; p[3] = (data_size >> 24) & 0xFF; p += 4;

    /* Generate a pronounced tap: 1.2kHz sine with sharp attack and smooth decay */
    for (int i = 0; i < TICK_NUM_SAMPLES; i++) {
        float t = (float)i / (float)TICK_SAMPLE_RATE;
        float phase = (float)i / (float)TICK_NUM_SAMPLES;
        /* Sharp attack (first 10% at full volume) then smooth exponential decay */
        float envelope;
        if (phase < 0.1f) {
            envelope = 1.0f; /* full volume attack */
        } else {
            float decay_phase = (phase - 0.1f) / 0.9f;
            envelope = (1.0f - decay_phase);
            envelope *= envelope * envelope; /* cubic decay — pronounced but not abrupt */
        }
        float sample = sinf(2.0f * 3.14159265f * 1200.0f * t) * envelope * 16000.0f;
        short s = (short)sample;
        p[0] = s & 0xFF;
        p[1] = (s >> 8) & 0xFF;
        p += 2;
    }
    s_tick_ready = 1;
}

static void nav_play_tick(void) {
    if (s_tick_ready) {
        PlaySound((LPCSTR)s_tick_wav, NULL, SND_MEMORY | SND_ASYNC | SND_NOSTOP);
    }
}

/* ========================================================================= */
/* Compass direction from dx/dz deltas                                       */
/* ========================================================================= */

static const char* compass_direction(f32 dx, f32 dz) {
    if (dx == 0.0f && dz == 0.0f) return "here";
    f32 ax = dx > 0 ? dx : -dx;
    f32 az = dz > 0 ? dz : -dz;
    if (az >= ax) {
        return (dz < 0) ? "north" : "south";
    } else {
        return (dx > 0) ? "east" : "west";
    }
}

#define UNITS_PER_STEP 20.0f

static int distance_to_steps(f32 dist) {
    int steps = (int)(dist / UNITS_PER_STEP + 0.5f);
    if (steps < 1 && dist > 1.0f) steps = 1;
    return steps;
}

/* ========================================================================= */
/* Structure/building name from npc_id                                       */
/* ========================================================================= */

static const char* structure_name_from_id(mActor_name_t id) {
    switch (id) {
        case HOUSE0: case HOUSE1: case HOUSE2: case HOUSE3:
            return "Player House";
        case SHOP0:            return "Nook's Cranny";
        case SHOP1:            return "Nook 'n' Go";
        case SHOP2:            return "Nookway";
        case SHOP3:            return "Nookington's";
        case POST_OFFICE:      return "Post Office";
        case TRAIN_STATION:    return "Train Station";
        case TRAIN0: case TRAIN1:
            return "Train";
        case POLICE_STATION:   return "Police Station";
        case WISHING_WELL:     return "Wishing Well";
        case DUMP:             return "Dump";
        case BROKER_TENT:      return "Crazy Redd's Tent";
        case FORTUNE_TENT:     return "Katrina's Tent";
        case DESIGNER_CAR:     return "Gracie's Car";
        case MUSEUM:           return "Museum";
        case NEEDLEWORK_SHOP:  return "Able Sisters";
        case TOUDAI:           return "Lighthouse";
        case TENT:             return "Tent";
        case COTTAGE_MY:       return "Island Cottage";
        case COTTAGE_NPC:
            return "Islander's Cottage";
        case BOAT:             return "Boat";
        case FLAG:             return "Flag";
        case BRIDGE_A0: case BRIDGE_A1:
            return "Bridge";
        case DOUZOU:           return "Train Station Statue";
        case KAMAKURA:         return "Igloo";
        case AEROBICS_RADIO:   return "Aerobics Radio";
        case LOTUS:            return "Pond";
        case GHOG:             return "Groundhog Stand";
        case WINDMILL0: case WINDMILL1: case WINDMILL2: case WINDMILL3: case WINDMILL4:
            return "Windmill";
        case PORT_SIGN:        return "Port Sign";
        default: break;
    }
    /* NPC houses: 0x5000-0x50EE */
    if (ITEM_IS_NPC_HOUSE(id)) {
        return "Villager House";
    }
    /* Signboards */
    if (id >= SIGN00 && id <= SIGN20) {
        return "Sign";
    }
    /* Event structures — still label generically if unknown */
    if (id >= STRUCTURE_START && id < STRUCTURE_END) {
        return "Structure";
    }
    return NULL;
}

/* Check if npc_id is a mailbox or gyroid (useful landmarks) */
static const char* prop_name_from_id(mActor_name_t id) {
    switch (id) {
        case ACTOR_PROP_MAILBOX0: case ACTOR_PROP_MAILBOX1:
        case ACTOR_PROP_MAILBOX2: case ACTOR_PROP_MAILBOX3:
            return "Mailbox";
        case ACTOR_PROP_HANIWA0: case ACTOR_PROP_HANIWA1:
        case ACTOR_PROP_HANIWA2: case ACTOR_PROP_HANIWA3:
            return "Gyroid";
        case TRAIN_DOOR:
            return "Train Station Door";
        default:
            return NULL;
    }
}

/* ========================================================================= */
/* Villager house name resolution                                            */
/* ========================================================================= */

/* Given an NPC house actor, find which villager lives there by comparing
 * the house's world position against each villager's home position in the
 * npclist. Returns the formatted name like "Murphy's House". */
static int resolve_npc_house_name(ACTOR* actor, char* buf, int buf_size) {
    xyz_t house_pos = actor->world.position;

    for (int i = 0; i < ANIMAL_NUM_MAX; i++) {
        mNpc_NpcList_c* npc = &Common_Get(npclist[i]);
        if (npc->name == EMPTY_NO) continue;

        f32 dx = house_pos.x - npc->house_position.x;
        f32 dz = house_pos.z - npc->house_position.z;
        f32 dist_sq = dx * dx + dz * dz;

        /* Houses are placed at the villager's home coords; allow 80 units tolerance */
        if (dist_sq < 80.0f * 80.0f) {
            u8* name_p = mNpc_GetNpcWorldNameP(npc->name);
            if (name_p) {
                int len = mMl_strlen(name_p, ANIMAL_NAME_LEN, CHAR_SPACE);
                if (len > 0) {
                    char name_utf8[24];
                    pc_acc_game_str_to_utf8(name_p, len, name_utf8, sizeof(name_utf8));
                    snprintf(buf, buf_size, "%s's House", name_utf8);
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* ========================================================================= */
/* Scan: populate s_results[] for the current category                       */
/* ========================================================================= */

static void nav_get_actor_name(ACTOR* actor, char* buf, int buf_size) {
    buf[0] = '\0';

    if (actor->part == ACTOR_PART_NPC) {
        /* NPC villager: get world name */
        u8 npc_name[ANIMAL_NAME_LEN];
        mNpc_GetNpcWorldName(npc_name, actor);
        int len = mMl_strlen(npc_name, ANIMAL_NAME_LEN, CHAR_SPACE);
        if (len > 0) {
            pc_acc_game_str_to_utf8(npc_name, len, buf, buf_size);
        }
        return;
    }

    /* NPC house: try to resolve the resident's name */
    if (ITEM_IS_NPC_HOUSE(actor->npc_id)) {
        if (resolve_npc_house_name(actor, buf, buf_size)) {
            return;
        }
        snprintf(buf, buf_size, "Villager House");
        return;
    }

    /* Check if it's a known structure (building) by npc_id */
    const char* sname = structure_name_from_id(actor->npc_id);
    if (sname) {
        snprintf(buf, buf_size, "%s", sname);
        return;
    }

    /* Check if it's a prop (mailbox, gyroid) */
    const char* pname = prop_name_from_id(actor->npc_id);
    if (pname) {
        snprintf(buf, buf_size, "%s", pname);
        return;
    }

    /* Regular items: only try mIN_copy_name_str for actual item types
     * (type 0=ITEM0, 2=ITEM1, 1=FTR0, 3=FTR1) */
    if (actor->part == ACTOR_PART_ITEM && actor->npc_id != 0) {
        int type = ITEM_NAME_GET_TYPE(actor->npc_id);
        if (type <= NAME_TYPE_FTR1) {
            u8 item_name[mIN_ITEM_NAME_LEN];
            mIN_copy_name_str(item_name, actor->npc_id);
            int len = mMl_strlen(item_name, mIN_ITEM_NAME_LEN, CHAR_SPACE);
            if (len > 0) {
                pc_acc_game_str_to_utf8(item_name, len, buf, buf_size);
            }
        }
    }
}

static int nav_matches_category(ACTOR* actor, NavCategory cat) {
    switch (cat) {
        case NAV_CAT_VILLAGERS:
            return (actor->part == ACTOR_PART_NPC);

        case NAV_CAT_BUILDINGS:
            /* Town buildings + props, but NOT villager/player houses */
            if (ITEM_IS_NPC_HOUSE(actor->npc_id)) return 0;
            if (actor->npc_id >= HOUSE0 && actor->npc_id <= HOUSE3) return 0;
            return (structure_name_from_id(actor->npc_id) != NULL ||
                    prop_name_from_id(actor->npc_id) != NULL);

        case NAV_CAT_HOUSES:
            /* Player houses + villager houses */
            if (ITEM_IS_NPC_HOUSE(actor->npc_id)) return 1;
            if (actor->npc_id >= HOUSE0 && actor->npc_id <= HOUSE3) return 1;
            return 0;

        case NAV_CAT_ITEMS:
            if (actor->part == ACTOR_PART_ITEM && actor->npc_id != 0) {
                int type = ITEM_NAME_GET_TYPE(actor->npc_id);
                return (type <= NAME_TYPE_FTR1);
            }
            return 0;

        default:
            return 0;
    }
}

static void nav_lock_current(void) {
    if (s_nav_index >= 0 && s_nav_index < s_result_count) {
        NavResult* r = &s_results[s_nav_index];
        strncpy(s_locked_name, r->name, sizeof(s_locked_name) - 1);
        s_locked_name[sizeof(s_locked_name) - 1] = '\0';
        s_locked_npc_id = r->actor ? r->actor->npc_id : r->item_id;
        s_locked_pos = r->world_pos;
        s_has_lock = 1;
    }
}

static void nav_clear_lock(void) {
    s_has_lock = 0;
    s_locked_name[0] = '\0';
    s_locked_npc_id = 0;
}

/* ========================================================================= */
/* Indoor shop item scanning — reads FG tile grid for purchasable items      */
/* ========================================================================= */

static int nav_is_shop_scene(int scene) {
    return (scene == SCENE_SHOP0 || scene == SCENE_CONVENI ||
            scene == SCENE_SUPER || scene == SCENE_DEPART ||
            scene == SCENE_DEPART_2 || scene == SCENE_NEEDLEWORK ||
            scene == SCENE_BROKER_SHOP);
}

static void nav_scan_room_items(xyz_t player_pos) {
    mActor_name_t* fg_p = mFI_BkNumtoUtFGTop(0, 0);
    if (!fg_p) return;

    for (int uz = 0; uz < UT_Z_NUM; uz++) {
        for (int ux = 0; ux < UT_X_NUM; ux++) {
            mActor_name_t item = fg_p[ux + uz * UT_X_NUM];

            /* Skip empty, sold-out, and reserved tiles */
            if (item == EMPTY_NO || item == RSV_NO) continue;
            if (item >= RSV_SHOP_SOLD_PAPER && item <= RSV_SHOP_SOLD_SIGNBOARD) continue;
            /* Skip RSV_SHOP_* placeholders that haven't been resolved yet */
            if (item >= RSV_SHOP_PAPER && item <= RSV_SHOP_SIGNBOARD) continue;

            /* Check if this is a real purchasable item */
            int type = ITEM_NAME_GET_TYPE(item);
            if (type > NAME_TYPE_FTR1) continue;

            /* Resolve item name */
            u8 item_name[mIN_ITEM_NAME_LEN];
            mIN_copy_name_str(item_name, item);
            int len = mMl_strlen(item_name, mIN_ITEM_NAME_LEN, CHAR_SPACE);
            if (len <= 0) continue;

            char name_utf8[32];
            pc_acc_game_str_to_utf8(item_name, len, name_utf8, sizeof(name_utf8));
            if (name_utf8[0] == '\0') continue;

            if (s_result_count >= NAV_MAX_RESULTS) return;

            /* Compute world position for this tile */
            xyz_t wpos;
            mFI_BkandUtNum2CenterWpos(&wpos, 0, 0, ux, uz);

            f32 ddx = wpos.x - player_pos.x;
            f32 ddz = wpos.z - player_pos.z;
            f32 dist = sqrtf(ddx * ddx + ddz * ddz);

            NavResult* r = &s_results[s_result_count];
            r->actor = NULL;
            r->item_id = item;
            r->world_pos = wpos;
            r->dist = dist;
            r->dx = ddx;
            r->dz = ddz;
            r->in_current_acre = 1; /* shop items are always in the current room */
            strncpy(r->name, name_utf8, sizeof(r->name) - 1);
            r->name[sizeof(r->name) - 1] = '\0';
            s_result_count++;
        }
    }
}

/* ========================================================================= */
/* Acre Ring Scanner helpers                                                 */
/* ========================================================================= */

/* Playable acre bounds: X 1-5, Z 1-6 (labeled A1-F5) */
#define ACRE_MIN_X 1
#define ACRE_MAX_X FG_BLOCK_X_NUM  /* 5 */
#define ACRE_MIN_Z 1
#define ACRE_MAX_Z FG_BLOCK_Z_NUM  /* 6 */

/* Classify an acre's block type into terrain feature strings */
static int ring_describe_terrain(int bi, char* buf, int buf_size, int block_type) {
    switch (block_type) {
        case mFM_BLOCK_TYPE_TRACKS_STATION:
            bi += snprintf(buf + bi, buf_size - bi, "Train Station"); return bi;
        case mFM_BLOCK_TYPE_TRACKS_SHOP:
            bi += snprintf(buf + bi, buf_size - bi, "Nook's Store"); return bi;
        case mFM_BLOCK_TYPE_TRACKS_POST_OFFICE:
            bi += snprintf(buf + bi, buf_size - bi, "Post Office"); return bi;
        case mFM_BLOCK_TYPE_SHRINE:
            bi += snprintf(buf + bi, buf_size - bi, "Wishing Well"); return bi;
        case mFM_BLOCK_TYPE_POLICE_BOX:
            bi += snprintf(buf + bi, buf_size - bi, "Police Station"); return bi;
        case mFM_BLOCK_TYPE_MUSEUM:
            bi += snprintf(buf + bi, buf_size - bi, "Museum"); return bi;
        case mFM_BLOCK_TYPE_NEEDLEWORK:
            bi += snprintf(buf + bi, buf_size - bi, "Able Sisters"); return bi;
        case mFM_BLOCK_TYPE_TRACKS_DUMP:
            bi += snprintf(buf + bi, buf_size - bi, "Dump"); return bi;
        case mFM_BLOCK_TYPE_PORT:
            bi += snprintf(buf + bi, buf_size - bi, "Dock"); return bi;
        case mFM_BLOCK_TYPE_PLAYER_HOUSE:
            bi += snprintf(buf + bi, buf_size - bi, "Player Houses"); return bi;
        default: break;
    }

    int wrote = 0;

    if (block_type == mFM_BLOCK_TYPE_BEACH ||
        block_type == mFM_BLOCK_TYPE_BEACH_RIVER ||
        block_type == mFM_BLOCK_TYPE_BEACH_RIVER_BRIDGE) {
        bi += snprintf(buf + bi, buf_size - bi, "Beach");
        wrote = 1;
    }

    if ((block_type >= mFM_BLOCK_TYPE_RIVER_SOUTH && block_type <= mFM_BLOCK_TYPE_RIVER_WEST_SOUTH) ||
        block_type == mFM_BLOCK_TYPE_BEACH_RIVER ||
        block_type == mFM_BLOCK_TYPE_TRACKS_RIVER) {
        if (wrote) bi += snprintf(buf + bi, buf_size - bi, ", ");
        bi += snprintf(buf + bi, buf_size - bi, "River");
        wrote = 1;
    }

    if ((block_type >= mFM_BLOCK_TYPE_RIVER_SOUTH_BRIDGE && block_type <= mFM_BLOCK_TYPE_RIVER_WEST_SOUTH_BRIDGE) ||
        block_type == mFM_BLOCK_TYPE_BEACH_RIVER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_TRACKS_RIVER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_STRAIGHT_CLIFF_HORIZONTAL_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_STRAIGHT_CLIFF_VERTICAL_RIGHT_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_STRAIGHT_CLIFF_TOP_RIGHT_CORNER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_EAST_CLIFF_TOP_RIGHT_CORNER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_EAST_CLIFF_TOP_LEFT_CORNER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_STRAIGHT_CLIFF_VERTICAL_LEFT_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_STRAIGHT_CLIFF_BOTTOM_LEFT_CORNER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_WEST_CLIFF_HORIZONTAL_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_WEST_CLIFF_TOP_RIGHT_CORNER_BRIDGE ||
        block_type == mFM_BLOCK_TYPE_RIVER_WEST_CLIFF_TOP_LEFT_CORNER_BRIDGE) {
        if (wrote) bi += snprintf(buf + bi, buf_size - bi, ", ");
        bi += snprintf(buf + bi, buf_size - bi, "River with Bridge");
        wrote = 1;
    }

    if ((block_type >= mFM_BLOCK_TYPE_CLIFF_HORIZONTAL && block_type <= mFM_BLOCK_TYPE_CLIFF_BOTTOM_LEFT_CORNER) ||
        (block_type >= mFM_BLOCK_TYPE_WATERFALL_STRAIGHT_CLIFF_HORIZONTAL && block_type <= mFM_BLOCK_TYPE_WATERFALL_WEST_CLIFF_BOTTOM_LEFT_CORNER)) {
        if (wrote) bi += snprintf(buf + bi, buf_size - bi, ", ");
        bi += snprintf(buf + bi, buf_size - bi, "Cliff");
        wrote = 1;
    }

    if (block_type >= mFM_BLOCK_TYPE_SLOPE_HORIZONTAL && block_type <= mFM_BLOCK_TYPE_SLOPE_BOTTOM_LEFT_CORNER) {
        if (wrote) bi += snprintf(buf + bi, buf_size - bi, ", ");
        bi += snprintf(buf + bi, buf_size - bi, "Ramp");
        wrote = 1;
    }

    if (block_type >= mFM_BLOCK_TYPE_POOL_SOUTH && block_type <= mFM_BLOCK_TYPE_POOL_WEST_SOUTH) {
        if (wrote) bi += snprintf(buf + bi, buf_size - bi, ", ");
        bi += snprintf(buf + bi, buf_size - bi, "Pond");
        wrote = 1;
    }

    if (block_type == mFM_BLOCK_TYPE_WATERFALL_STRAIGHT_CLIFF_HORIZONTAL ||
        block_type == mFM_BLOCK_TYPE_WATERFALL_STRAIGHT_CLIFF_BOTTOM_RIGHT_CORNER ||
        block_type == mFM_BLOCK_TYPE_WATERFALL_STRAIGHT_CLIFF_TOP_LEFT_CORNER ||
        block_type == mFM_BLOCK_TYPE_WATERFALL_EAST_CLIFF_BOTTOM_RIGHT_CORNER ||
        block_type == mFM_BLOCK_TYPE_WATERFALL_EAST_CLIFF_VERTICAL_RIGHT ||
        block_type == mFM_BLOCK_TYPE_WATERFALL_WEST_CLIFF_VERTICAL_LEFT ||
        block_type == mFM_BLOCK_TYPE_WATERFALL_WEST_CLIFF_BOTTOM_LEFT_CORNER) {
        if (wrote) bi += snprintf(buf + bi, buf_size - bi, ", ");
        bi += snprintf(buf + bi, buf_size - bi, "Waterfall");
        wrote = 1;
    }

    if (!wrote) {
        bi += snprintf(buf + bi, buf_size - bi, "Open");
    }

    return bi;
}

static int ring_describe_houses(int bi, char* buf, int buf_size, int bx, int bz) {
    for (int i = 0; i < ANIMAL_NUM_MAX; i++) {
        Animal_c* animal = &Save_Get(animals[i]);
        if (animal->id.npc_id == EMPTY_NO) continue;
        if (animal->home_info.block_x == bx && animal->home_info.block_z == bz) {
            u8* name_p = mNpc_GetNpcWorldNameP(animal->id.npc_id);
            if (name_p) {
                int len = mMl_strlen(name_p, ANIMAL_NAME_LEN, CHAR_SPACE);
                if (len > 0) {
                    char name_utf8[24];
                    pc_acc_game_str_to_utf8(name_p, len, name_utf8, sizeof(name_utf8));
                    bi += snprintf(buf + bi, buf_size - bi, ", %s's House", name_utf8);
                }
            }
        }
    }
    return bi;
}

static int ring_describe_villagers(int bi, char* buf, int buf_size, int bx, int bz) {
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return bi;

    Actor_info* ai = &play->actor_info;
    ACTOR* actor = ai->list[ACTOR_PART_NPC].actor;

    while (actor != NULL) {
        int abx, abz;
        mFI_Wpos2BlockNum(&abx, &abz, actor->world.position);
        if (abx == bx && abz == bz) {
            u8 npc_name[ANIMAL_NAME_LEN];
            mNpc_GetNpcWorldName(npc_name, actor);
            int len = mMl_strlen(npc_name, ANIMAL_NAME_LEN, CHAR_SPACE);
            if (len > 0) {
                char name_utf8[24];
                pc_acc_game_str_to_utf8(npc_name, len, name_utf8, sizeof(name_utf8));
                bi += snprintf(buf + bi, buf_size - bi, ", %s is here", name_utf8);
            }
        }
        actor = actor->next_actor;
    }
    return bi;
}

static int ring_describe_bridge(int bi, char* buf, int buf_size, int bx, int bz) {
    PlusBridge_c* bridge = &Save_Get(bridge);
    if (bridge->exists && bridge->block_x == bx && bridge->block_z == bz) {
        bi += snprintf(buf + bi, buf_size - bi, ", Extra Bridge");
    }
    return bi;
}

/* ========================================================================= */
/* Scan: populate s_results[] for the current category                       */
/* ========================================================================= */

/* Helper: check if acre (bx,bz) is in the playable area */
static int acre_in_bounds(int bx, int bz) {
    return (bx >= ACRE_MIN_X && bx <= ACRE_MAX_X && bz >= ACRE_MIN_Z && bz <= ACRE_MAX_Z);
}

/* Spatial label for player house position within acre B-3 */
static const char* house_slot_label(int slot) {
    switch (slot) {
        case mHS_HOUSE0: return "top left";
        case mHS_HOUSE1: return "top right";
        case mHS_HOUSE2: return "bottom left";
        case mHS_HOUSE3: return "bottom right";
        default: return "";
    }
}

/* Helper: add a result entry from an actor */
static void nav_add_actor_result(ACTOR* actor, xyz_t player_pos, int pbx, int pbz) {
    if (s_result_count >= NAV_MAX_RESULTS) return;
    char name[64];
    nav_get_actor_name(actor, name, sizeof(name));
    if (name[0] == '\0') return;
    int abx, abz;
    mFI_Wpos2BlockNum(&abx, &abz, actor->world.position);
    NavResult* r = &s_results[s_result_count];
    r->actor = actor;
    r->item_id = actor->npc_id;
    r->world_pos = actor->world.position;
    r->dist = actor->player_distance_xz;
    r->dx = actor->world.position.x - player_pos.x;
    r->dz = actor->world.position.z - player_pos.z;
    r->in_current_acre = (abx == pbx && abz == pbz) ? 1 : 0;
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->name[sizeof(r->name) - 1] = '\0';
    s_result_count++;
}

/* Helper: add a non-actor result (from save data or block types) */
static void nav_add_data_result(const char* name, xyz_t pos, xyz_t player_pos, int in_current) {
    if (s_result_count >= NAV_MAX_RESULTS) return;

    NavResult* r = &s_results[s_result_count];
    r->actor = NULL;
    r->item_id = 0;
    r->world_pos = pos;
    r->dx = pos.x - player_pos.x;
    r->dz = pos.z - player_pos.z;
    r->dist = sqrtf(r->dx * r->dx + r->dz * r->dz);
    r->in_current_acre = in_current ? 1 : 0;
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->name[sizeof(r->name) - 1] = '\0';
    s_result_count++;
}

/* Check if we already have a result with the same name at a similar position */
static int nav_result_exists(const char* name, xyz_t pos, f32 tolerance) {
    for (int i = 0; i < s_result_count; i++) {
        if (strcmp(s_results[i].name, name) == 0) {
            f32 ddx = s_results[i].world_pos.x - pos.x;
            f32 ddz = s_results[i].world_pos.z - pos.z;
            if (ddx * ddx + ddz * ddz < tolerance * tolerance) return 1;
        }
    }
    return 0;
}

static void nav_refresh(void) {
    s_result_count = 0;

    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t player_pos = player->actor_class.world.position;
    int scene = Save_Get(scene_no);

    /* Get player's current acre */
    int pbx = 0, pbz = 0;
    if (scene == SCENE_FG) {
        mFI_Wpos2BlockNum(&pbx, &pbz, player_pos);
    }

    /* Indoor shop: scan FG grid for items */
    if (s_category == NAV_CAT_ITEMS && nav_is_shop_scene(scene)) {
        nav_scan_room_items(player_pos);
    }
    /* Terrain: full town scan from block types */
    else if (s_category == NAV_CAT_TERRAIN && scene == SCENE_FG) {
        for (int bz = ACRE_MIN_Z; bz <= ACRE_MAX_Z; bz++) {
            for (int bx = ACRE_MIN_X; bx <= ACRE_MAX_X; bx++) {
                int bt = data_combi_table[Save_Get(combi_table[bz][bx]).combination_type].type;
                char terrain[64];
                ring_describe_terrain(0, terrain, sizeof(terrain), bt);

                PlusBridge_c* bridge = &Save_Get(bridge);
                char label[64];
                if (bridge->exists && bridge->block_x == bx && bridge->block_z == bz) {
                    char terrain2[64];
                    snprintf(terrain2, sizeof(terrain2), "%s, Extra Bridge", terrain);
                    snprintf(label, sizeof(label), "%c-%d: %s", 'A' + (bz - 1), bx, terrain2);
                } else {
                    snprintf(label, sizeof(label), "%c-%d: %s", 'A' + (bz - 1), bx, terrain);
                }

                xyz_t center;
                mFI_BkandUtNum2CenterWpos(&center, bx, bz, 8, 8);
                nav_add_data_result(label, center, player_pos,
                                    (bx == pbx && bz == pbz) ? 1 : 0);
            }
        }
    }
    /* Full town scan for Villagers, Buildings, Houses */
    else if (s_category == NAV_CAT_VILLAGERS || s_category == NAV_CAT_BUILDINGS ||
             s_category == NAV_CAT_HOUSES) {
        /* Step 1: scan loaded actors */
        Actor_info* ai = &play->actor_info;

        for (int part = 0; part < ACTOR_PART_NUM; part++) {
            ACTOR* actor = ai->list[part].actor;
            while (actor != NULL) {
                if (nav_matches_category(actor, s_category)) {
                    f32 dist = actor->player_distance_xz;

                    if (scene == SCENE_FG) {
                        /* Outdoors: include all actors in playable area */
                        int abx, abz;
                        mFI_Wpos2BlockNum(&abx, &abz, actor->world.position);
                        if (acre_in_bounds(abx, abz) && dist > 0.1f) {
                            nav_add_actor_result(actor, player_pos, pbx, pbz);
                        }
                    } else {
                        /* Indoors: use radius scan */
                        if (dist <= NAV_SCAN_RADIUS && dist > 0.1f) {
                            nav_add_actor_result(actor, player_pos, pbx, pbz);
                        }
                    }
                }
                actor = actor->next_actor;
            }
        }

        /* Step 2: Villagers — supplement with npclist data for ALL spawned
         * villagers in the town (covers unloaded actors in distant acres) */
        if (s_category == NAV_CAT_VILLAGERS && scene == SCENE_FG) {
            for (int i = 0; i < ANIMAL_NUM_MAX; i++) {
                mNpc_NpcList_c* npc = &Common_Get(npclist[i]);
                if (npc->name == EMPTY_NO) continue;
                if (npc->appear_flag == 0) continue;

                char vname[64];
                u8* name_p = mNpc_GetNpcWorldNameP(npc->name);
                if (!name_p) continue;
                int len = mMl_strlen(name_p, ANIMAL_NAME_LEN, CHAR_SPACE);
                if (len <= 0) continue;
                pc_acc_game_str_to_utf8(name_p, len, vname, sizeof(vname));
                if (vname[0] == '\0') continue;

                if (nav_result_exists(vname, npc->position, 120.0f)) continue;

                int nbx, nbz;
                mFI_Wpos2BlockNum(&nbx, &nbz, npc->position);
                nav_add_data_result(vname, npc->position, player_pos,
                                    (nbx == pbx && nbz == pbz) ? 1 : 0);
            }
        }

        /* Step 3: Houses — supplement with save data for ALL villager houses
         * in the entire town, plus all 4 player house slots */
        if (s_category == NAV_CAT_HOUSES && scene == SCENE_FG) {
            /* Villager houses from save data */
            for (int i = 0; i < ANIMAL_NUM_MAX; i++) {
                Animal_c* animal = &Save_Get(animals[i]);
                if (animal->id.npc_id == EMPTY_NO) continue;

                int hbx = animal->home_info.block_x;
                int hbz = animal->home_info.block_z;

                char hname[64];
                u8* name_p = mNpc_GetNpcWorldNameP(animal->id.npc_id);
                if (name_p) {
                    int len = mMl_strlen(name_p, ANIMAL_NAME_LEN, CHAR_SPACE);
                    if (len > 0) {
                        char name_utf8[24];
                        pc_acc_game_str_to_utf8(name_p, len, name_utf8, sizeof(name_utf8));
                        snprintf(hname, sizeof(hname), "%s's House", name_utf8);
                    } else continue;
                } else continue;

                xyz_t hpos;
                mFI_BkandUtNum2CenterWpos(&hpos, hbx, hbz,
                                           animal->home_info.ut_x, animal->home_info.ut_z);

                if (nav_result_exists(hname, hpos, 120.0f)) continue;

                nav_add_data_result(hname, hpos, player_pos,
                                    (hbx == pbx && hbz == pbz) ? 1 : 0);
            }

            /* All 4 player house slots with spatial labels */
            int cur_player = Common_Get(player_no);
            for (int slot = 0; slot < mHS_HOUSE_NUM; slot++) {
                int pl = mHS_get_pl_no(slot);
                const char* pos_label = house_slot_label(slot);

                /* Find the acre with player houses */
                int house_bx = -1, house_bz = -1;
                for (int bz = ACRE_MIN_Z; bz <= ACRE_MAX_Z && house_bx < 0; bz++) {
                    for (int bx = ACRE_MIN_X; bx <= ACRE_MAX_X; bx++) {
                        int bt = data_combi_table[Save_Get(combi_table[bz][bx]).combination_type].type;
                        if (bt == mFM_BLOCK_TYPE_PLAYER_HOUSE) {
                            house_bx = bx;
                            house_bz = bz;
                            break;
                        }
                    }
                }
                if (house_bx < 0) continue; /* no player house acre found */

                /* Compute position within the acre based on slot quadrant.
                 * Top-left=slot0 (ut 4,4), top-right=slot1 (ut 12,4),
                 * bottom-left=slot2 (ut 4,12), bottom-right=slot3 (ut 12,12) */
                int ut_x = (slot & 1) ? 12 : 4;
                int ut_z = (slot & 2) ? 12 : 4;
                xyz_t hpos;
                mFI_BkandUtNum2CenterWpos(&hpos, house_bx, house_bz, ut_x, ut_z);

                char hname[64];
                if (pl == cur_player && pl >= 0) {
                    snprintf(hname, sizeof(hname), "Your house, %s", pos_label);
                } else if (pl >= 0 && pl < PLAYER_NUM) {
                    /* Another player's house — use their name from private data */
                    Private_c* priv = &Save_Get(private_data[pl]);
                    char pname[24];
                    int len = mMl_strlen(priv->player_ID.player_name, PLAYER_NAME_LEN, CHAR_SPACE);
                    if (len > 0) {
                        pc_acc_game_str_to_utf8(priv->player_ID.player_name, len, pname, sizeof(pname));
                        snprintf(hname, sizeof(hname), "%s's house, %s", pname, pos_label);
                    } else {
                        snprintf(hname, sizeof(hname), "Player house, %s", pos_label);
                    }
                } else {
                    snprintf(hname, sizeof(hname), "Empty house, %s", pos_label);
                }

                if (!nav_result_exists(hname, hpos, 120.0f)) {
                    nav_add_data_result(hname, hpos, player_pos,
                                        (house_bx == pbx && house_bz == pbz) ? 1 : 0);
                }
            }
        }

        /* Step 4: Buildings — supplement with block-type data for ALL 30 acres */
        if (s_category == NAV_CAT_BUILDINGS && scene == SCENE_FG) {
            for (int bz = ACRE_MIN_Z; bz <= ACRE_MAX_Z; bz++) {
                for (int bx = ACRE_MIN_X; bx <= ACRE_MAX_X; bx++) {
                    int bt = data_combi_table[Save_Get(combi_table[bz][bx]).combination_type].type;
                    const char* bname = NULL;

                    switch (bt) {
                        case mFM_BLOCK_TYPE_TRACKS_STATION:    bname = "Train Station"; break;
                        case mFM_BLOCK_TYPE_TRACKS_SHOP:       bname = "Nook's Store"; break;
                        case mFM_BLOCK_TYPE_TRACKS_POST_OFFICE: bname = "Post Office"; break;
                        case mFM_BLOCK_TYPE_SHRINE:            bname = "Wishing Well"; break;
                        case mFM_BLOCK_TYPE_POLICE_BOX:        bname = "Police Station"; break;
                        case mFM_BLOCK_TYPE_MUSEUM:            bname = "Museum"; break;
                        case mFM_BLOCK_TYPE_NEEDLEWORK:        bname = "Able Sisters"; break;
                        case mFM_BLOCK_TYPE_TRACKS_DUMP:       bname = "Dump"; break;
                        case mFM_BLOCK_TYPE_PORT:              bname = "Dock"; break;
                        default: break;
                    }

                    if (bname) {
                        xyz_t center;
                        mFI_BkandUtNum2CenterWpos(&center, bx, bz, 8, 8);
                        if (!nav_result_exists(bname, center, 640.0f)) {
                            int in_cur = (bx == pbx && bz == pbz) ? 1 : 0;
                            nav_add_data_result(bname, center, player_pos, in_cur);
                        }
                    }
                }
            }
        }
    }
    /* Items: current area loaded actors */
    else {
        Actor_info* ai = &play->actor_info;

        for (int part = 0; part < ACTOR_PART_NUM; part++) {
            ACTOR* actor = ai->list[part].actor;
            while (actor != NULL) {
                if (nav_matches_category(actor, s_category)) {
                    f32 dist = actor->player_distance_xz;
                    if (dist <= NAV_SCAN_RADIUS && dist > 0.1f) {
                        nav_add_actor_result(actor, player_pos, pbx, pbz);
                    }
                }
                actor = actor->next_actor;
            }
        }
    }

    /* Sort by distance (simple insertion sort for small N) */
    for (int i = 1; i < s_result_count; i++) {
        NavResult tmp = s_results[i];
        int j = i - 1;
        while (j >= 0 && s_results[j].dist > tmp.dist) {
            s_results[j + 1] = s_results[j];
            j--;
        }
        s_results[j + 1] = tmp;
    }

    /* Re-find locked target after re-sort */
    if (s_has_lock && s_result_count > 0) {
        int found = -1;
        f32 best_dist = 999999.0f;

        /* Match by npc_id/item_id + closest to last known position */
        for (int i = 0; i < s_result_count; i++) {
            NavResult* r = &s_results[i];
            mActor_name_t rid = r->actor ? r->actor->npc_id : r->item_id;
            if (rid == s_locked_npc_id) {
                xyz_t rpos = r->actor ? r->actor->world.position : r->world_pos;
                f32 ddx = rpos.x - s_locked_pos.x;
                f32 ddz = rpos.z - s_locked_pos.z;
                f32 d = ddx * ddx + ddz * ddz;
                if (d < best_dist) {
                    best_dist = d;
                    found = i;
                }
            }
        }

        /* Fallback: match by name */
        if (found < 0) {
            for (int i = 0; i < s_result_count; i++) {
                if (strcmp(s_results[i].name, s_locked_name) == 0) {
                    found = i;
                    break;
                }
            }
        }

        if (found >= 0) {
            s_nav_index = found;
            /* Update locked position to track moving targets (NPCs) */
            NavResult* fr = &s_results[found];
            s_locked_pos = fr->actor ? fr->actor->world.position : fr->world_pos;
        } else {
            /* Target genuinely gone — announce and clear lock */
            pc_acc_speak_queue("Target lost");
            nav_clear_lock();
            s_nav_index = s_result_count > 0 ? 0 : -1;
        }
    } else if (!s_has_lock) {
        /* No lock — clamp index */
        if (s_nav_index >= s_result_count) {
            s_nav_index = s_result_count > 0 ? 0 : -1;
        }
    }
}

/* ========================================================================= */
/* NPC proximity auto-announcements                                          */
/* ========================================================================= */

static int proximity_already_announced(mActor_name_t npc_id) {
    for (int i = 0; i < s_proximity_count; i++) {
        if (s_proximity_announced[i] == npc_id) return 1;
    }
    return 0;
}

static void proximity_mark_announced(mActor_name_t npc_id) {
    if (s_proximity_count < MAX_PROXIMITY_TRACKED) {
        s_proximity_announced[s_proximity_count++] = npc_id;
    }
}

static void nav_check_proximity(void) {
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    int scene = Save_Get(scene_no);

    /* Reset proximity tracking on scene change */
    if (scene != s_prev_scene_nav) {
        s_proximity_count = 0;
        s_prev_scene_nav = scene;
    }

    /* Only check outdoors */
    if (scene != SCENE_FG) return;

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t player_pos = player->actor_class.world.position;
    Actor_info* ai = &play->actor_info;
    ACTOR* actor = ai->list[ACTOR_PART_NPC].actor;

    while (actor != NULL) {
        f32 dist = actor->player_distance_xz;

        if (dist <= NAV_PROXIMITY_RADIUS && dist > 5.0f) {
            if (!proximity_already_announced(actor->npc_id)) {
                char name[32];
                u8 npc_name[ANIMAL_NAME_LEN];
                mNpc_GetNpcWorldName(npc_name, actor);
                int len = mMl_strlen(npc_name, ANIMAL_NAME_LEN, CHAR_SPACE);
                if (len > 0) {
                    int wrote = pc_acc_game_str_to_utf8(npc_name, len, name, sizeof(name));
                    if (wrote > 0) {
                        f32 pdx = actor->world.position.x - player_pos.x;
                        f32 pdz = actor->world.position.z - player_pos.z;
                        const char* dir = compass_direction(pdx, pdz);

                        char buf[96];
                        snprintf(buf, sizeof(buf), "%s nearby, %s", name, dir);
                        pc_acc_speak_queue(buf);
                        proximity_mark_announced(actor->npc_id);
                    }
                }
            }
        } else if (dist > NAV_PROXIMITY_RADIUS * 2.0f) {
            /* If NPC moved far away, allow re-announcement when they come back */
            for (int i = 0; i < s_proximity_count; i++) {
                if (s_proximity_announced[i] == actor->npc_id) {
                    s_proximity_announced[i] = s_proximity_announced[--s_proximity_count];
                    break;
                }
            }
        }

        actor = actor->next_actor;
    }
}

/* ========================================================================= */
/* Forward declarations for pathfinding (defined later)                      */
/* ========================================================================= */

typedef struct {
    int dir;    /* 0-7, index into s_dir_names8 */
    int count;  /* number of tiles in this direction */
} PathSeg;

#define MAX_PATH_SEGS 64

static int nav_pathfind(int sx, int sz, int gx, int gz, PathSeg* segs, int max_segs);
static void nav_speak_path(const char* target_name, PathSeg* segs, int seg_count);

/* ========================================================================= */
/* A* pathfinding on the tile grid                                           */
/* ========================================================================= */

/* Grid dimensions: 7 blocks * 16 tiles = 112 wide, 10 * 16 = 160 tall */
#define NAV_GRID_W (BLOCK_X_NUM * UT_X_NUM)
#define NAV_GRID_H (BLOCK_Z_NUM * UT_Z_NUM)
#define NAV_GRID_SIZE (NAV_GRID_W * NAV_GRID_H)
#define NAV_IDX(x, z) ((z) * NAV_GRID_W + (x))
#define NAV_HEAP_MAX 8192

/* Pathfinding workspace — malloc'd on demand, freed after use */
typedef struct {
    f32* g;        /* g-scores (NAV_GRID_SIZE) */
    f32* f;        /* f-scores (NAV_GRID_SIZE) */
    u16* from;     /* parent index (NAV_GRID_SIZE), 0xFFFF = none */
    u8* closed;    /* closed set flags (NAV_GRID_SIZE) */
    u8* in_open;   /* open set membership (NAV_GRID_SIZE) */
    u16* heap;     /* binary min-heap of grid indices (NAV_HEAP_MAX) */
    int heap_size;
} NavPF;

static NavPF* pf_alloc(void) {
    NavPF* pf = (NavPF*)malloc(sizeof(NavPF));
    if (!pf) return NULL;
    pf->g       = (f32*)malloc(NAV_GRID_SIZE * sizeof(f32));
    pf->f       = (f32*)malloc(NAV_GRID_SIZE * sizeof(f32));
    pf->from    = (u16*)malloc(NAV_GRID_SIZE * sizeof(u16));
    pf->closed  = (u8*)calloc(NAV_GRID_SIZE, 1);
    pf->in_open = (u8*)calloc(NAV_GRID_SIZE, 1);
    pf->heap    = (u16*)malloc(NAV_HEAP_MAX * sizeof(u16));
    pf->heap_size = 0;
    if (!pf->g || !pf->f || !pf->from || !pf->closed || !pf->in_open || !pf->heap) {
        free(pf->g); free(pf->f); free(pf->from);
        free(pf->closed); free(pf->in_open); free(pf->heap);
        free(pf);
        return NULL;
    }
    /* Init g-scores to infinity */
    for (int i = 0; i < NAV_GRID_SIZE; i++) {
        pf->g[i] = 1e9f;
        pf->f[i] = 1e9f;
        pf->from[i] = 0xFFFF;
    }
    return pf;
}

static void pf_free(NavPF* pf) {
    if (!pf) return;
    free(pf->g); free(pf->f); free(pf->from);
    free(pf->closed); free(pf->in_open); free(pf->heap);
    free(pf);
}

/* Binary min-heap operations keyed by f-score */
static void pf_heap_push(NavPF* pf, u16 idx) {
    if (pf->heap_size >= NAV_HEAP_MAX) return;
    int i = pf->heap_size++;
    pf->heap[i] = idx;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (pf->f[pf->heap[i]] < pf->f[pf->heap[parent]]) {
            u16 tmp = pf->heap[i];
            pf->heap[i] = pf->heap[parent];
            pf->heap[parent] = tmp;
            i = parent;
        } else break;
    }
}

static u16 pf_heap_pop(NavPF* pf) {
    u16 result = pf->heap[0];
    pf->heap[0] = pf->heap[--pf->heap_size];
    int i = 0;
    for (;;) {
        int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
        if (left < pf->heap_size && pf->f[pf->heap[left]] < pf->f[pf->heap[smallest]])
            smallest = left;
        if (right < pf->heap_size && pf->f[pf->heap[right]] < pf->f[pf->heap[smallest]])
            smallest = right;
        if (smallest == i) break;
        u16 tmp = pf->heap[i];
        pf->heap[i] = pf->heap[smallest];
        pf->heap[smallest] = tmp;
        i = smallest;
    }
    return result;
}

static int nav_tile_walkable(int ux, int uz) {
    return mCoBG_Unit2CheckNpc(ux, uz) ? 1 : 0;
}

/* Manhattan distance heuristic (4-directional movement) */
static f32 nav_heuristic(int x0, int z0, int x1, int z1) {
    return (f32)(abs(x1 - x0) + abs(z1 - z0));
}

/* 4-direction neighbor offsets (cardinal only) */
static const int s_dx8[] = { 0,  1,  0, -1 };
static const int s_dz8[] = { -1,  0,  1,  0 };
static const f32  s_cost8[] = { 1.0f, 1.0f, 1.0f, 1.0f };
#define NAV_DIR_COUNT 4

/* Direction name for each of the 4 neighbors */
static const char* s_dir_names8[] = {
    "north", "east", "south", "west"
};

/* Snap an unwalkable tile to the nearest walkable neighbor (spiral search).
 * Used to find the door/entrance of buildings/houses. */
static int nav_snap_to_walkable(int* ux, int* uz) {
    if (nav_tile_walkable(*ux, *uz)) return 1;
    /* Spiral outward up to 5 tiles */
    for (int r = 1; r <= 5; r++) {
        for (int dz = -r; dz <= r; dz++) {
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dz) != r) continue; /* only perimeter */
                int nx = *ux + dx;
                int nz = *uz + dz;
                if (nav_tile_walkable(nx, nz)) {
                    *ux = nx;
                    *uz = nz;
                    return 1;
                }
            }
        }
    }
    return 0;
}

#define MAX_RAW_PATH 512

/* Run A* from (sx,sz) to (gx,gz). Returns number of direction segments,
 * or 0 if no path found. Writes segments to segs[]. */
static int nav_pathfind(int sx, int sz, int gx, int gz, PathSeg* segs, int max_segs) {
    /* Snap start and goal to nearest walkable tiles (handles building interiors) */
    nav_snap_to_walkable(&sx, &sz);
    nav_snap_to_walkable(&gx, &gz);

    NavPF* pf = pf_alloc();
    if (!pf) return 0;

    u16 start = (u16)NAV_IDX(sx, sz);
    u16 goal  = (u16)NAV_IDX(gx, gz);

    pf->g[start] = 0.0f;
    pf->f[start] = nav_heuristic(sx, sz, gx, gz);
    pf_heap_push(pf, start);
    pf->in_open[start] = 1;

    int found = 0;
    int iterations = 0;
    int max_iterations = NAV_GRID_SIZE; /* upper bound safety */

    while (pf->heap_size > 0 && iterations++ < max_iterations) {
        u16 cur = pf_heap_pop(pf);
        if (cur == goal) { found = 1; break; }
        if (pf->closed[cur]) continue;
        pf->closed[cur] = 1;

        int cx = cur % NAV_GRID_W;
        int cz = cur / NAV_GRID_W;

        for (int d = 0; d < NAV_DIR_COUNT; d++) {
            int nx = cx + s_dx8[d];
            int nz = cz + s_dz8[d];
            if (nx < 0 || nx >= NAV_GRID_W || nz < 0 || nz >= NAV_GRID_H) continue;

            u16 ni = (u16)NAV_IDX(nx, nz);
            if (pf->closed[ni]) continue;
            if (!nav_tile_walkable(nx, nz)) continue;

            f32 tentative_g = pf->g[cur] + s_cost8[d];
            if (tentative_g < pf->g[ni]) {
                pf->from[ni] = cur;
                pf->g[ni] = tentative_g;
                pf->f[ni] = tentative_g + nav_heuristic(nx, nz, gx, gz);
                if (!pf->in_open[ni]) {
                    pf_heap_push(pf, ni);
                    pf->in_open[ni] = 1;
                } else {
                    /* Lazy decrease-key: just push again; closed check filters dupes */
                    pf_heap_push(pf, ni);
                }
            }
        }
    }

    int seg_count = 0;

    if (found) {
        /* Reconstruct path backwards */
        u16 raw_path[MAX_RAW_PATH];
        int path_len = 0;
        u16 cur = goal;
        while (cur != start && path_len < MAX_RAW_PATH) {
            raw_path[path_len++] = cur;
            if (pf->from[cur] == 0xFFFF) break;
            cur = pf->from[cur];
        }

        /* Reverse to get start→goal order */
        for (int i = 0; i < path_len / 2; i++) {
            u16 tmp = raw_path[i];
            raw_path[i] = raw_path[path_len - 1 - i];
            raw_path[path_len - 1 - i] = tmp;
        }

        /* Convert to direction segments */
        int prev_x = sx, prev_z = sz;
        int cur_dir = -1;
        int cur_count = 0;

        for (int i = 0; i < path_len; i++) {
            int px = raw_path[i] % NAV_GRID_W;
            int pz = raw_path[i] / NAV_GRID_W;
            int ddx = px - prev_x;
            int ddz = pz - prev_z;

            /* Find direction index */
            int dir = -1;
            for (int d = 0; d < NAV_DIR_COUNT; d++) {
                if (s_dx8[d] == ddx && s_dz8[d] == ddz) { dir = d; break; }
            }

            if (dir >= 0) {
                if (dir == cur_dir) {
                    cur_count++;
                } else {
                    if (cur_dir >= 0 && seg_count < max_segs) {
                        segs[seg_count].dir = cur_dir;
                        segs[seg_count].count = cur_count;
                        seg_count++;
                    }
                    cur_dir = dir;
                    cur_count = 1;
                }
            }

            prev_x = px;
            prev_z = pz;
        }
        /* Final segment */
        if (cur_dir >= 0 && seg_count < max_segs) {
            segs[seg_count].dir = cur_dir;
            segs[seg_count].count = cur_count;
            seg_count++;
        }
    }

    pf_free(pf);
    return found ? seg_count : -1; /* -1 = no path, 0 = already there */
}

/* Build TTS string from path segments */
static void nav_speak_path(const char* target_name, PathSeg* segs, int seg_count) {
    char buf[256];
    int bi = 0;

    if (seg_count == 0) {
        snprintf(buf, sizeof(buf), "Already at %s", target_name);
        pc_acc_speak_interrupt(buf);
        return;
    }

    bi += snprintf(buf + bi, sizeof(buf) - bi, "Path to %s: ", target_name);

    int total_tiles = 0;
    for (int i = 0; i < seg_count && bi < (int)sizeof(buf) - 30; i++) {
        if (i > 0) bi += snprintf(buf + bi, sizeof(buf) - bi, ", ");
        bi += snprintf(buf + bi, sizeof(buf) - bi, "%d %s",
                       segs[i].count, s_dir_names8[segs[i].dir]);
        total_tiles += segs[i].count;
    }

    snprintf(buf + bi, sizeof(buf) - bi, ". %d steps total.", total_tiles);
    pc_acc_speak_interrupt(buf);
}

/* ========================================================================= */
/* Main update — called once per frame                                       */
/* ========================================================================= */

void pc_acc_nav_update(void) {
    if (!pc_acc_is_active()) return;

    /* One-time init for tile tick WAV */
    if (!s_tick_ready) {
        nav_generate_tick_wav();
    }

    /* Tile tick: play a subtle click when the player crosses into a new tile. */
    {
        GAME_PLAY* play = (GAME_PLAY*)gamePT;
        if (play && Save_Get(scene_no) == SCENE_FG) {
            PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
            if (player) {
                int tx, tz;
                if (mFI_Wpos2UtNum(&tx, &tz, player->actor_class.world.position)) {
                    if (s_prev_tile_x >= 0 && (tx != s_prev_tile_x || tz != s_prev_tile_z)) {
                        nav_play_tick();
                    }
                    s_prev_tile_x = tx;
                    s_prev_tile_z = tz;
                }
            }
        } else {
            /* Reset tile tracking when not outdoors */
            s_prev_tile_x = -1;
            s_prev_tile_z = -1;
        }
    }

    /* NPC proximity auto-announcements (always active) */
    nav_check_proximity();

    /* Helper macro: announce a nav result with direction only if in a neighboring acre */
#define NAV_ANNOUNCE_RESULT(r) do { \
    int _steps = distance_to_steps((r)->dist); \
    char _buf[160]; \
    if ((r)->in_current_acre) { \
        snprintf(_buf, sizeof(_buf), "%s, %d steps", (r)->name, _steps); \
    } else { \
        const char* _dir = compass_direction((r)->dx, (r)->dz); \
        snprintf(_buf, sizeof(_buf), "%s, %s, %d steps", (r)->name, _dir, _steps); \
    } \
    pc_acc_speak_interrupt(_buf); \
} while(0)

    const u8* keys = SDL_GetKeyboardState(NULL);
    if (!keys) return;

    u8 shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    u8 l_key = keys[SDL_SCANCODE_L];
    u8 j_key = keys[SDL_SCANCODE_J];
    u8 k_key = keys[SDL_SCANCODE_K];

    /* Shift+L: next category */
    if (shift && l_key && !s_prev_l) {
        s_category = (s_category + 1) % NAV_CAT_COUNT;
        s_nav_index = -1;
        nav_clear_lock();
        nav_refresh();
        char buf[64];
        snprintf(buf, sizeof(buf), "%s, %d found", s_cat_names[s_category], s_result_count);
        pc_acc_speak_interrupt(buf);
    }
    /* Shift+J: prev category */
    else if (shift && j_key && !s_prev_j) {
        s_category = (s_category + NAV_CAT_COUNT - 1) % NAV_CAT_COUNT;
        s_nav_index = -1;
        nav_clear_lock();
        nav_refresh();
        char buf[64];
        snprintf(buf, sizeof(buf), "%s, %d found", s_cat_names[s_category], s_result_count);
        pc_acc_speak_interrupt(buf);
    }
    /* L (no shift): next item — moves selection and locks new target */
    else if (!shift && l_key && !s_prev_l) {
        nav_clear_lock();
        nav_refresh();
        if (s_result_count == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "No %s found", s_cat_names[s_category]);
            pc_acc_speak_interrupt(buf);
        } else {
            s_nav_index = (s_nav_index + 1) % s_result_count;
            nav_lock_current();
            NAV_ANNOUNCE_RESULT(&s_results[s_nav_index]);
        }
    }
    /* J (no shift): prev item — moves selection and locks new target */
    else if (!shift && j_key && !s_prev_j) {
        nav_clear_lock();
        nav_refresh();
        if (s_result_count == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "No %s found", s_cat_names[s_category]);
            pc_acc_speak_interrupt(buf);
        } else {
            s_nav_index--;
            if (s_nav_index < 0) s_nav_index = s_result_count - 1;
            nav_lock_current();
            NAV_ANNOUNCE_RESULT(&s_results[s_nav_index]);
        }
    }
    /* Shift+K: A* pathfinding to selected target */
    else if (shift && k_key && !s_prev_k) {
        nav_refresh();
        if (s_result_count == 0 || s_nav_index < 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "No %s selected", s_cat_names[s_category]);
            pc_acc_speak_interrupt(buf);
        } else if (Save_Get(scene_no) != SCENE_FG) {
            pc_acc_speak_interrupt("Pathfinding only works outdoors");
        } else {
            NavResult* r = &s_results[s_nav_index];
            GAME_PLAY* play = (GAME_PLAY*)gamePT;
            PLAYER_ACTOR* player = play ? get_player_actor_withoutCheck(play) : NULL;
            if (!player) {
                pc_acc_speak_interrupt("Cannot pathfind right now");
            } else {
                xyz_t p_pos = player->actor_class.world.position;
                xyz_t t_pos = r->world_pos;

                int p_ux, p_uz, t_ux, t_uz;
                if (mFI_Wpos2UtNum(&p_ux, &p_uz, p_pos) &&
                    mFI_Wpos2UtNum(&t_ux, &t_uz, t_pos)) {

                    PathSeg segs[MAX_PATH_SEGS];
                    int result = nav_pathfind(p_ux, p_uz, t_ux, t_uz, segs, MAX_PATH_SEGS);

                    if (result >= 0) {
                        nav_speak_path(r->name, segs, result);
                    } else {
                        char buf[128];
                        snprintf(buf, sizeof(buf), "No path to %s", r->name);
                        pc_acc_speak_interrupt(buf);
                    }
                } else {
                    pc_acc_speak_interrupt("Cannot determine position");
                }
            }
        }
    }
    /* K (no shift): A* pathfinding directions to selected target */
    else if (!shift && k_key && !s_prev_k) {
        nav_refresh();
        if (s_result_count == 0 || s_nav_index < 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "No %s selected", s_cat_names[s_category]);
            pc_acc_speak_interrupt(buf);
        } else if (Save_Get(scene_no) != SCENE_FG) {
            /* Indoors: fall back to simple compass direction */
            NavResult* r = &s_results[s_nav_index];
            int steps = distance_to_steps(r->dist);
            const char* dir = compass_direction(r->dx, r->dz);
            char buf[128];
            snprintf(buf, sizeof(buf), "%s, %s, %d steps", r->name, dir, steps);
            pc_acc_speak_interrupt(buf);
        } else {
            NavResult* r = &s_results[s_nav_index];
            GAME_PLAY* play2 = (GAME_PLAY*)gamePT;
            PLAYER_ACTOR* player2 = play2 ? get_player_actor_withoutCheck(play2) : NULL;
            if (!player2) {
                pc_acc_speak_interrupt("Cannot pathfind right now");
            } else {
                xyz_t p_pos = player2->actor_class.world.position;
                xyz_t t_pos = r->world_pos;

                int p_ux, p_uz, t_ux, t_uz;
                if (mFI_Wpos2UtNum(&p_ux, &p_uz, p_pos) &&
                    mFI_Wpos2UtNum(&t_ux, &t_uz, t_pos)) {

                    PathSeg segs[MAX_PATH_SEGS];
                    int result = nav_pathfind(p_ux, p_uz, t_ux, t_uz, segs, MAX_PATH_SEGS);

                    if (result >= 0) {
                        nav_speak_path(r->name, segs, result);
                    } else {
                        /* No walkable path — fall back to straight-line direction */
                        int steps = distance_to_steps(r->dist);
                        const char* dir = compass_direction(r->dx, r->dz);
                        char buf[128];
                        snprintf(buf, sizeof(buf), "No walkable path to %s. Straight line: %s, %d steps", r->name, dir, steps);
                        pc_acc_speak_interrupt(buf);
                    }
                } else {
                    pc_acc_speak_interrupt("Cannot determine position");
                }
            }
        }
    }

    s_prev_l = l_key;
    s_prev_j = j_key;
    s_prev_k = k_key;
}

int pc_acc_nav_is_active(void) {
    return pc_acc_is_active() ? 1 : 0;
}

#else
/* Non-Windows stubs */
void pc_acc_nav_update(void) {}
int pc_acc_nav_is_active(void) { return 0; }
#endif
