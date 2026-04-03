/* pc_acc_gameplay.c - Accessibility for fishing, bug catching, fossil digging.
 *
 * FISHING (hooks in ac_uki_move.c_inc):
 *   Cast  → "Line cast"
 *   Nibble (gyo_status=2) → "Nibble" — do NOT press A
 *   Bite   (gyo_status=3) → "Bite now!" — MUST press A immediately
 *   Catch  (gyo_status=6) → "Caught [fish name]!"
 *   Escape (gyo_status→0) → "Fish got away"
 *   CRITICAL: The original game has NO audio distinction between nibble and
 *   bite — only vibration intensity differs. The bite TTS cue is the single
 *   most important accessibility feature for fishing.
 *
 * BUG CATCHING (per-frame scan + hooks):
 *   Continuously scans 8 insect slots when net is equipped.
 *   Announces bug name on first detection in range.
 *   Patience-based proximity alerts:
 *     0-50   → "[name] nearby, calm"
 *     51-75  → "[name] nervous, slow down"
 *     76-89  → "[name] about to flee, stop"
 *     90+    → "[name] fleeing"
 *   Auto-approach: moves toward bug at safe speed using per-species spook data.
 *   Auto-catch: swings net when within catch range.
 *   F10: toggle auto-catch on/off
 *
 * FOSSIL DIGGING (hotkey + hooks):
 *   F9: scan nearby SHINE_SPOTs, announce count + direction to nearest
 *   Scene load / day change: rescan all SHINE_SPOTs
 *   Dig result: announce unearthed item name
 */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_gameplay.h"
#include "pc_acc_msg.h"
#include "m_common_data.h"
#include "m_private.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "m_field_info.h"
#include "m_field_make.h"
#include "m_scene_table.h"
#include "m_player_lib.h"
#include "m_player.h"
#include "m_play.h"
#include "m_actor.h"
#include "ac_gyoei.h"
#include "ac_insect_h.h"
#include "m_collision_bg.h"
#include "game.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

extern GAME* gamePT;

/* ========================================================================= */
/* Helpers                                                                    */
/* ========================================================================= */

static void item_to_utf8(mActor_name_t item, char* out, int out_sz) {
    u8 name_buf[mIN_ITEM_NAME_LEN];
    mIN_copy_name_str(name_buf, item);
    pc_acc_game_str_to_utf8(name_buf, mIN_ITEM_NAME_LEN, out, out_sz);
}

/* ========================================================================= */
/* 1. FISHING                                                                 */
/* ========================================================================= */

static const char* fishing_item_name(int gyo_type) {
    static mActor_name_t fish_data[] = {
        ITM_FISH00, ITM_FISH01, ITM_FISH02, ITM_FISH03, ITM_FISH04,
        ITM_FISH05, ITM_FISH06, ITM_FISH07, ITM_FISH08, ITM_FISH09,
        ITM_FISH10, ITM_FISH11, ITM_FISH12, ITM_FISH13, ITM_FISH14,
        ITM_FISH15, ITM_FISH16, ITM_FISH17, ITM_FISH18, ITM_FISH19,
        ITM_FISH20, ITM_FISH21, ITM_FISH22, ITM_FISH23, ITM_FISH24,
        ITM_FISH25, ITM_FISH26, ITM_FISH27, ITM_FISH28, ITM_FISH29,
        ITM_FISH30, ITM_FISH31, ITM_FISH32, ITM_FISH33, ITM_FISH34,
        ITM_FISH35, ITM_FISH36, ITM_FISH37, ITM_FISH38, ITM_FISH39,
        ITM_FISH39,
        ITM_DUST0_EMPTY_CAN, ITM_DUST1_BOOT, ITM_DUST2_OLD_TIRE,
        ITM_FISH22,
    };
    static char utf8_buf[128];
    if (gyo_type < 0 || gyo_type >= aGYO_TYPE_EXTENDED_NUM) return "unknown fish";
    item_to_utf8(fish_data[gyo_type], utf8_buf, sizeof(utf8_buf));
    return utf8_buf;
}

void pc_acc_fishing_cast(void) {
    pc_acc_speak_interrupt("Line cast");
}

void pc_acc_fishing_nibble(void) {
    pc_acc_speak_interrupt("Nibble");
}

void pc_acc_fishing_bite(void) {
    pc_acc_speak_interrupt("Bite now!");
}

void pc_acc_fishing_catch(int gyo_type) {
    char buf[160];
    snprintf(buf, sizeof(buf), "Caught %s!", fishing_item_name(gyo_type));
    pc_acc_speak_interrupt(buf);
}

void pc_acc_fishing_escape(void) {
    pc_acc_speak_interrupt("Fish got away");
}

/* ========================================================================= */
/* 2. BUG CATCHING                                                            */
/* ========================================================================= */

/* Per-species spook distance offsets — mirrors catch_ME_data[] exactly from
 * ac_insect_move.c_inc:75-117. Added to base distance of 120 world units.
 * Index = insect type enum value. */
static const f32 s_spook_offset[41] = {
    /* 0  common butterfly    */  0.0f,
    /* 1  yellow butterfly    */  0.0f,
    /* 2  tiger butterfly     */  0.0f,
    /* 3  purple butterfly    */  0.0f,
    /* 4  robust cicada       */ 10.0f,
    /* 5  walker cicada       */ 10.0f,
    /* 6  evening cicada      */ 10.0f,
    /* 7  brown cicada        */  0.0f,
    /* 8  bee                 */ 10.0f,
    /* 9  common dragonfly    */  0.0f,
    /* 10 red dragonfly       */  0.0f,
    /* 11 darner dragonfly    */  0.0f,
    /* 12 banded dragonfly    */  0.0f,
    /* 13 long locust         */  0.0f,
    /* 14 migratory locust    */ 20.0f,
    /* 15 cricket             */-20.0f,
    /* 16 grasshopper         */-20.0f,
    /* 17 bell cricket        */-20.0f,
    /* 18 pine cricket        */-20.0f,
    /* 19 drone beetle        */-20.0f,
    /* 20 dynastid beetle     */-20.0f,
    /* 21 flat stag beetle    */-20.0f,
    /* 22 jewel beetle        */  0.0f,
    /* 23 longhorn beetle     */  0.0f,
    /* 24 ladybug             */-20.0f,
    /* 25 spotted ladybug     */-20.0f,
    /* 26 mantis              */-20.0f,
    /* 27 firefly             */  0.0f,
    /* 28 cockroach           */  0.0f,
    /* 29 saw stag beetle     */  0.0f,
    /* 30 mountain beetle     */  0.0f,
    /* 31 giant beetle        */  0.0f,
    /* 32 snail               */  0.0f,
    /* 33 mole cricket        */  0.0f,
    /* 34 pond skater         */  0.0f,
    /* 35 bagworm             */  0.0f,
    /* 36 pill bug            */  0.0f,
    /* 37 spider              */  0.0f,
    /* 38 ant                 */  0.0f,
    /* 39 mosquito            */  0.0f,
    /* 40 spirit              */  0.0f,
};

#define SPOOK_BASE 120.0f  /* aINS_MAX_STRESS_DIST = 3 * 40 */
#define NET_CATCH_RANGE_REGULAR 39.0f  /* 15 + 24 (net radius + insect catch radius) */
#define NET_CATCH_RANGE_GOLD    45.0f  /* 21 + 24 */

/* Bug tracking state per insect slot */
#define BUG_SLOTS 8  /* slots 0-7 are normal insects */

typedef struct {
    int announced;       /* have we announced this bug's presence? */
    int last_patience_band; /* 0=calm, 1=nervous, 2=flee-warning, 3=fleeing */
    int announce_cooldown;  /* frames until next patience announcement */
} BugTrack;

static BugTrack s_bug_track[BUG_SLOTS];
static int s_auto_catch_enabled = 1; /* on by default when net equipped */
static int s_last_scene_bugs = -1;

/* Auto-approach/auto-catch output: read by pc_pad.c via getter */
static s8 s_auto_stick_x = 0;
static s8 s_auto_stick_y = 0;
static int s_auto_press_a = 0;

/* Find the insect controller actor by scanning ACTOR_PART_CONTROL list */
static aINS_CTRL_ACTOR* find_insect_controller(void) {
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return NULL;

    Actor_info* info = &play->actor_info;
    ACTOR* actor;

    for (actor = info->list[ACTOR_PART_CONTROL].actor; actor != NULL; actor = actor->next_actor) {
        if (actor->id == mAc_PROFILE_INSECT) {
            return (aINS_CTRL_ACTOR*)actor;
        }
    }
    return NULL;
}

static f32 get_spook_distance(int type) {
    if (type < 0 || type >= (int)(sizeof(s_spook_offset) / sizeof(s_spook_offset[0])))
        return SPOOK_BASE;
    return SPOOK_BASE + s_spook_offset[type];
}

static int player_has_net(void) {
    Private_c* priv = Common_Get(now_private);
    if (!priv || priv->equipment == EMPTY_NO) return 0;

    mActor_name_t equip = priv->equipment;
    /* Net items: ITM_NET and ITM_GOLDEN_NET */
    return (equip == ITM_NET || equip == ITM_GOLDEN_NET);
}

static int player_has_gold_net(void) {
    Private_c* priv = Common_Get(now_private);
    return (priv && priv->equipment == ITM_GOLDEN_NET);
}

static int get_patience_band(f32 patience) {
    if (patience >= 90.0f) return 3; /* fleeing */
    if (patience >= 76.0f) return 2; /* about to flee */
    if (patience >= 51.0f) return 1; /* nervous */
    return 0; /* calm */
}

static const char* patience_band_text(int band) {
    switch (band) {
        case 0: return "calm";
        case 1: return "nervous, slow down";
        case 2: return "about to flee, stop moving";
        case 3: return "fleeing";
        default: return "";
    }
}

void pc_acc_bug_caught(u16 item) {
    char utf8_buf[128];
    char buf[192];
    item_to_utf8(item, utf8_buf, sizeof(utf8_buf));
    snprintf(buf, sizeof(buf), "Caught %s!", utf8_buf);
    pc_acc_speak_interrupt(buf);
}

void pc_acc_bug_missed(void) {
    pc_acc_speak_queue("Miss");
}

/* Per-frame bug scanning — called when outdoors and net is equipped */
static void update_bug_scanning(void) {
    /* Reset auto-input each frame — will be set if auto-approach is active */
    s_auto_stick_x = 0;
    s_auto_stick_y = 0;
    s_auto_press_a = 0;

    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    int scene = Save_Get(scene_no);

    /* Reset tracking on scene change */
    if (scene != s_last_scene_bugs) {
        memset(s_bug_track, 0, sizeof(s_bug_track));
        s_last_scene_bugs = scene;
    }

    /* Only scan outdoors with net equipped */
    if (scene != SCENE_FG) return;
    if (!player_has_net()) return;

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    aINS_CTRL_ACTOR* ctrl = find_insect_controller();
    if (!ctrl) return;

    xyz_t ppos = player->actor_class.world.position;
    f32 catch_range = player_has_gold_net() ? NET_CATCH_RANGE_GOLD : NET_CATCH_RANGE_REGULAR;

    /* Track nearest bug for auto-approach */
    f32 nearest_dist = 999999.0f;
    int nearest_slot = -1;

    for (int i = 0; i < BUG_SLOTS; i++) {
        aINS_INSECT_ACTOR* insect = &ctrl->insect_actor[i];
        BugTrack* bt = &s_bug_track[i];

        if (!insect->exist_flag) {
            /* Slot went empty — reset tracking */
            bt->announced = 0;
            bt->last_patience_band = -1;
            bt->announce_cooldown = 0;
            continue;
        }

        xyz_t ipos = insect->tools_actor.actor_class.world.position;
        f32 dx = ipos.x - ppos.x;
        f32 dz = ipos.z - ppos.z;
        f32 dist = sqrtf(dx * dx + dz * dz);

        /* Track bugs within the entire acre (~640 world units).
         * Auto-approach will begin navigating toward them automatically. */
        if (dist > 640.0f) {
            bt->announced = 0;
            bt->last_patience_band = -1;
            continue;
        }

        /* Get bug name */
        char bug_name[128];
        item_to_utf8(insect->item, bug_name, sizeof(bug_name));

        /* First detection announcement */
        if (!bt->announced) {
            char buf[192];
            snprintf(buf, sizeof(buf), "%s nearby", bug_name);
            pc_acc_speak_interrupt(buf);
            bt->announced = 1;
            bt->last_patience_band = get_patience_band(insect->patience);
            bt->announce_cooldown = 90; /* 1.5 seconds before next patience update */
        }

        /* Patience-based proximity alerts */
        int band = get_patience_band(insect->patience);
        if (bt->announce_cooldown > 0) {
            bt->announce_cooldown--;
        }

        if (band != bt->last_patience_band && bt->announce_cooldown == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %s", bug_name, patience_band_text(band));
            pc_acc_speak_interrupt(buf);
            bt->last_patience_band = band;
            bt->announce_cooldown = 90; /* cooldown between patience announcements */
        }

        /* Track nearest for auto-approach */
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_slot = i;
        }
    }

    /* Auto-approach and auto-catch logic */
    if (!s_auto_catch_enabled) return;
    if (nearest_slot < 0) return;

    aINS_INSECT_ACTOR* target = &ctrl->insect_actor[nearest_slot];
    xyz_t tpos = target->tools_actor.actor_class.world.position;
    f32 tdx = tpos.x - ppos.x;
    f32 tdz = tpos.z - ppos.z;
    f32 spook = get_spook_distance(target->type);

    /* Skip auto-approach if bug is over unwalkable terrain (water, cliff, etc.) */
    {
        int bug_ux = (int)(tpos.x / 40.0f);
        int bug_uz = (int)(tpos.z / 40.0f);
        if (!mCoBG_Unit2CheckNpc(bug_ux, bug_uz)) {
            return;
        }
    }

    /* Auto-swing when within catch range */
    if (nearest_dist <= catch_range) {
        s_auto_press_a = 1;
        s_auto_stick_x = 0;
        s_auto_stick_y = 0;
        if (s_bug_track[nearest_slot].announce_cooldown == 0) {
            pc_acc_speak_interrupt("In range");
            s_bug_track[nearest_slot].announce_cooldown = 120;
        }
        return;
    }

    /* Auto-approach: steer toward bug with obstacle avoidance.
     * Check walkability of tiles ahead and steer around obstacles. */
    {
        f32 approach_magnitude;
        if (nearest_dist < spook * 0.8f) {
            approach_magnitude = 20.0f; /* creep inside spook range */
        } else if (nearest_dist < spook * 1.2f) {
            approach_magnitude = 40.0f; /* moderate near spook boundary */
        } else {
            approach_magnitude = 72.0f; /* full speed when far */
        }

        if (nearest_dist > 1.0f) {
            /* Get current tile position */
            int p_ux = (int)(ppos.x / 40.0f);
            int p_uz = (int)(ppos.z / 40.0f);

            /* Desired direction toward bug (normalized) */
            f32 nx = tdx / nearest_dist;
            f32 nz = tdz / nearest_dist;

            /* Check the tile one step ahead in the desired direction */
            int step_x = (nx > 0.3f) ? 1 : (nx < -0.3f) ? -1 : 0;
            int step_z = (nz > 0.3f) ? 1 : (nz < -0.3f) ? -1 : 0;

            int ahead_walkable = mCoBG_Unit2CheckNpc(p_ux + step_x, p_uz + step_z);

            if (ahead_walkable) {
                /* Direct path is clear — go straight */
                s_auto_stick_x = (s8)(nx * approach_magnitude);
                s_auto_stick_y = (s8)(-nz * approach_magnitude);
            } else {
                /* Obstacle ahead — try to go around it.
                 * Check perpendicular directions and pick the walkable one
                 * that gets us closest to the target. */
                f32 best_score = -999999.0f;
                int best_dx = 0, best_dz = 0;

                /* 4 cardinal directions */
                static const int try_dx[] = { 1, -1, 0, 0 };
                static const int try_dz[] = { 0, 0, 1, -1 };

                for (int d = 0; d < 4; d++) {
                    int cx = p_ux + try_dx[d];
                    int cz = p_uz + try_dz[d];
                    if (!mCoBG_Unit2CheckNpc(cx, cz)) continue;

                    /* Score: how much does this direction reduce distance to target?
                     * Higher = better. Use dot product with target direction. */
                    f32 score = (f32)try_dx[d] * nx + (f32)try_dz[d] * nz;
                    if (score > best_score) {
                        best_score = score;
                        best_dx = try_dx[d];
                        best_dz = try_dz[d];
                    }
                }

                if (best_dx != 0 || best_dz != 0) {
                    s_auto_stick_x = (s8)((f32)best_dx * approach_magnitude);
                    s_auto_stick_y = (s8)((f32)(-best_dz) * approach_magnitude);
                }
                /* else: completely stuck, stick stays at 0 */
            }
        }
    }
}

/* ========================================================================= */
/* 3. FOSSIL DIGGING / BURIED ITEMS                                           */
/* ========================================================================= */

void pc_acc_dig_unearthed(u16 item) {
    char utf8_buf[128];
    char buf[192];
    item_to_utf8(item, utf8_buf, sizeof(utf8_buf));
    snprintf(buf, sizeof(buf), "Unearthed %s", utf8_buf);
    pc_acc_speak_interrupt(buf);
}

static void scan_buried_items(void) {
    GAME_PLAY* play = (GAME_PLAY*)gamePT;
    if (!play) return;

    int scene = Save_Get(scene_no);
    if (scene != SCENE_FG) {
        pc_acc_speak_interrupt("Buried item scan only works outdoors");
        return;
    }

    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    if (!player) return;

    xyz_t player_pos = player->actor_class.world.position;
    f32 px = player_pos.x;
    f32 pz = player_pos.z;

    int found_count = 0;
    f32 nearest_dist = 999999.0f;
    f32 nearest_dx = 0.0f, nearest_dz = 0.0f;

    /* Scan 9x9 tile area around player (±4 tiles = ±160 world units) */
    for (int dz = -4; dz <= 4; dz++) {
        for (int dx = -4; dx <= 4; dx++) {
            xyz_t check_pos;
            check_pos.x = px + (f32)(dx * mFI_UNIT_BASE_SIZE);
            check_pos.y = player_pos.y;
            check_pos.z = pz + (f32)(dz * mFI_UNIT_BASE_SIZE);

            mActor_name_t* fg = mFI_GetUnitFG(check_pos);
            if (fg && *fg == SHINE_SPOT) {
                f32 ddx = check_pos.x - px;
                f32 ddz = check_pos.z - pz;
                f32 dist = sqrtf(ddx * ddx + ddz * ddz);

                found_count++;
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_dx = ddx;
                    nearest_dz = ddz;
                }
            }
        }
    }

    if (found_count == 0) {
        pc_acc_speak_interrupt("No buried items nearby");
        return;
    }

    /* Compass direction */
    const char* dir;
    f32 angle_deg = atan2f(nearest_dx, -nearest_dz) * (180.0f / 3.14159265f);
    if      (angle_deg >= -22.5f  && angle_deg <  22.5f)  dir = "north";
    else if (angle_deg >=  22.5f  && angle_deg <  67.5f)  dir = "northeast";
    else if (angle_deg >=  67.5f  && angle_deg < 112.5f)  dir = "east";
    else if (angle_deg >= 112.5f  && angle_deg < 157.5f)  dir = "southeast";
    else if (angle_deg >= 157.5f  || angle_deg < -157.5f) dir = "south";
    else if (angle_deg >= -157.5f && angle_deg < -112.5f) dir = "southwest";
    else if (angle_deg >= -112.5f && angle_deg < -67.5f)  dir = "west";
    else dir = "northwest";

    int steps = (int)(nearest_dist / 20.0f + 0.5f);
    if (steps < 1 && nearest_dist > 1.0f) steps = 1;

    char result[256];
    if (found_count == 1) {
        snprintf(result, sizeof(result),
                 "1 buried item. To your %s, %d steps", dir, steps);
    } else {
        snprintf(result, sizeof(result),
                 "%d buried items. Nearest to your %s, %d steps",
                 found_count, dir, steps);
    }

    pc_acc_speak_interrupt(result);
}

/* Called on scene load and day change to rescan */
void pc_acc_gameplay_refresh_buried(void) {
    /* Just a notification for now — the actual scan is on-demand via F9.
     * Could auto-announce count on scene entry if desired. */
}

/* ========================================================================= */
/* Per-frame update                                                           */
/* ========================================================================= */

static u8 s_prev_f2 = 0;
static u8 s_prev_f3 = 0;

void pc_acc_gameplay_update(void) {
    if (!pc_acc_is_active()) return;
    if (!gamePT) return;

    const u8* keys = SDL_GetKeyboardState(NULL);
    if (!keys) return;

    /* F2: scan for buried items */
    u8 f2 = keys[SDL_SCANCODE_F2];
    if (f2 && !s_prev_f2) {
        scan_buried_items();
    }
    s_prev_f2 = f2;

    /* F3: toggle auto-catch */
    u8 f3 = keys[SDL_SCANCODE_F3];
    if (f3 && !s_prev_f3) {
        s_auto_catch_enabled = !s_auto_catch_enabled;
        pc_acc_speak_interrupt(s_auto_catch_enabled ? "Auto catch on" : "Auto catch off");
    }
    s_prev_f3 = f3;

    /* Bug scanning (only when outdoors and net equipped) */
    update_bug_scanning();
}

void pc_acc_gameplay_get_auto_input(s8* stick_x, s8* stick_y, int* press_a) {
    *stick_x = s_auto_stick_x;
    *stick_y = s_auto_stick_y;
    *press_a = s_auto_press_a;
    /* Clear press_a after reading so it's only a single-frame pulse */
    s_auto_press_a = 0;
}

#else /* non-Windows stubs */

void pc_acc_gameplay_update(void) {}
void pc_acc_fishing_cast(void) {}
void pc_acc_fishing_nibble(void) {}
void pc_acc_fishing_bite(void) {}
void pc_acc_fishing_catch(int gyo_type) { (void)gyo_type; }
void pc_acc_fishing_escape(void) {}
void pc_acc_bug_caught(u16 item) { (void)item; }
void pc_acc_bug_missed(void) {}
void pc_acc_dig_unearthed(u16 item) { (void)item; }
void pc_acc_gameplay_refresh_buried(void) {}
void pc_acc_gameplay_get_auto_input(s8* sx, s8* sy, int* pa) { *sx = 0; *sy = 0; *pa = 0; }

#endif
