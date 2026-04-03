/* pc_acc_furniture.c - Furniture Mode accessibility for indoor rooms.
 *
 * Provides a virtual grid cursor for scanning room contents,
 * enhanced narration for push/pull/rotate, blocked-move feedback,
 * room overview, and wallpaper/carpet announcements.
 *
 * Toggle: F key
 * Cursor: Arrow keys (while Furniture Mode is active)
 * Room overview: R key
 * Wallpaper/carpet: Shift+F
 */

#include "pc_accessibility.h"

#ifdef _WIN32

#include "pc_acc_furniture.h"
#include "pc_acc_msg.h"
#include "m_field_info.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "m_room_type.h"
#include "m_scene_table.h"
#include "m_common_data.h"
#include "m_font.h"
#include "ac_furniture.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================= */
/* Configuration                                                             */
/* ========================================================================= */

#define FTR_BUF_SIZE 512
#define FTR_NAME_SIZE 48
#define GRID_MAX 16

/* ========================================================================= */
/* State                                                                     */
/* ========================================================================= */

static int s_enabled = 0;          /* Furniture Mode toggle */
static int s_cursor_x = 1;        /* Virtual cursor grid X */
static int s_cursor_z = 1;        /* Virtual cursor grid Z */

/* Key edge detection */
static u8 s_prev_f = 0;
static u8 s_prev_r = 0;
static u8 s_prev_up = 0;
static u8 s_prev_down = 0;
static u8 s_prev_left = 0;
static u8 s_prev_right = 0;

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

/* Check if current scene is an indoor room where furniture can exist */
static int ftr_is_indoor_scene(void) {
    int scene = Save_Get(scene_no);
    return mSc_IS_SCENE_PLAYER_ROOM(scene) ||
           scene == SCENE_NPC_HOUSE;
}

/* Dynamically find valid grid bounds by scanning the FG grid for walls.
 * Returns min/max X and Z of non-wall cells. */
static void ftr_get_room_bounds(int* min_x, int* max_x, int* min_z, int* max_z) {
    mActor_name_t* fg = mFI_BkNum2UtFGTop_layer(0, 0, 0);
    *min_x = GRID_MAX; *max_x = 0;
    *min_z = GRID_MAX; *max_z = 0;

    if (!fg) {
        /* Fallback to standard room bounds */
        *min_x = 1; *max_x = 8; *min_z = 1; *max_z = 8;
        return;
    }

    for (int z = 0; z < GRID_MAX; z++) {
        for (int x = 0; x < GRID_MAX; x++) {
            mActor_name_t cell = fg[x + z * GRID_MAX];
            if (cell != RSV_WALL_NO) {
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (z < *min_z) *min_z = z;
                if (z > *max_z) *max_z = z;
            }
        }
    }

    if (*min_x > *max_x) {
        /* No non-wall cells found — fallback */
        *min_x = 1; *max_x = 8; *min_z = 1; *max_z = 8;
    }
}

/* Get furniture name as UTF-8 from an item ID. Returns 0 if not furniture. */
static int ftr_get_name(mActor_name_t item, char* buf, int buf_size) {
    buf[0] = '\0';
    if (!ITEM_IS_FTR(item)) return 0;

    u8 game_name[mIN_ITEM_NAME_LEN];
    mIN_copy_name_str(game_name, item);
    int len = mMl_strlen(game_name, mIN_ITEM_NAME_LEN, CHAR_SPACE);
    if (len <= 0) return 0;

    return pc_acc_game_str_to_utf8(game_name, len, buf, buf_size);
}

/* Get furniture name from a FTR_ACTOR->name value (furniture index, NOT item ID).
 * Converts index to item ID via mRmTp_FtrIdx2FtrItemNo, then looks up the name. */
static int ftr_get_name_from_idx(u16 ftr_idx, char* buf, int buf_size) {
    mActor_name_t item = mRmTp_FtrIdx2FtrItemNo(ftr_idx, mRmTp_DIRECT_SOUTH);
    return ftr_get_name(item, buf, buf_size);
}

/* Get carpet or wallpaper name as UTF-8 from a raw index and base item ID. */
static int ftr_get_decor_name(int idx, mActor_name_t base_item, char* buf, int buf_size) {
    buf[0] = '\0';
    if (idx < 0) return 0;

    mActor_name_t item = base_item + idx;
    u8 game_name[mIN_ITEM_NAME_LEN];
    mIN_copy_name_str(game_name, item);
    int len = mMl_strlen(game_name, mIN_ITEM_NAME_LEN, CHAR_SPACE);
    if (len <= 0) return 0;

    return pc_acc_game_str_to_utf8(game_name, len, buf, buf_size);
}

/* Get size description from shape type */
static const char* ftr_shape_size_str(u8 shape) {
    switch (shape) {
        case aFTR_SHAPE_TYPEA:    return "1 by 1";
        case aFTR_SHAPE_TYPEB_0:  return "1 by 2";
        case aFTR_SHAPE_TYPEB_90: return "1 by 2";
        case aFTR_SHAPE_TYPEB_180:return "1 by 2";
        case aFTR_SHAPE_TYPEB_270:return "1 by 2";
        case aFTR_SHAPE_TYPEC:    return "2 by 2";
        default:                  return "";
    }
}

/* Get facing direction from shape type (only meaningful for 1x2) */
static const char* ftr_shape_facing_str(u8 shape) {
    switch (shape) {
        case aFTR_SHAPE_TYPEB_0:   return "south";
        case aFTR_SHAPE_TYPEB_90:  return "east";
        case aFTR_SHAPE_TYPEB_180: return "north";
        case aFTR_SHAPE_TYPEB_270: return "west";
        default: return NULL;
    }
}

/* Build wall adjacency string for a grid position.
 * Dynamically checks against room bounds. */
static int ftr_wall_adjacency(int ut_x, int ut_z, char* buf, int buf_size) {
    int min_x, max_x, min_z, max_z;
    ftr_get_room_bounds(&min_x, &max_x, &min_z, &max_z);

    int oi = 0;
    int count = 0;

    if (ut_z == min_z) {
        oi += snprintf(buf + oi, buf_size - oi, "%snorth wall", count++ ? " and " : "");
    }
    if (ut_z == max_z) {
        oi += snprintf(buf + oi, buf_size - oi, "%ssouth wall", count++ ? " and " : "");
    }
    if (ut_x == min_x) {
        oi += snprintf(buf + oi, buf_size - oi, "%swest wall", count++ ? " and " : "");
    }
    if (ut_x == max_x) {
        oi += snprintf(buf + oi, buf_size - oi, "%seast wall", count++ ? " and " : "");
    }

    return oi;
}

/* Build adjacent furniture string — check all 4 cardinal neighbors. */
static int ftr_adjacent_furniture(int ut_x, int ut_z, char* buf, int buf_size) {
    mActor_name_t* fg = mFI_BkNum2UtFGTop_layer(0, 0, 0);
    if (!fg) return 0;

    static const int dx[] = { 0, 0, -1, 1 };
    static const int dz[] = { -1, 1, 0, 0 };
    static const char* dir_name[] = { "north", "south", "west", "east" };

    int oi = 0;
    for (int i = 0; i < 4; i++) {
        int nx = ut_x + dx[i];
        int nz = ut_z + dz[i];
        if (nx < 0 || nx >= GRID_MAX || nz < 0 || nz >= GRID_MAX) continue;

        mActor_name_t neighbor = fg[nx + nz * GRID_MAX];
        if (ITEM_IS_FTR(neighbor)) {
            char name[FTR_NAME_SIZE];
            if (ftr_get_name(neighbor, name, sizeof(name)) > 0) {
                if (oi > 0) oi += snprintf(buf + oi, buf_size - oi, ". ");
                oi += snprintf(buf + oi, buf_size - oi, "%s to the %s", name, dir_name[i]);
            }
        }
    }
    return oi;
}

/* ========================================================================= */
/* Virtual cursor                                                            */
/* ========================================================================= */

static void ftr_speak_cursor_cell(void) {
    if (!pc_acc_is_active()) return;

    mActor_name_t* fg = mFI_BkNum2UtFGTop_layer(0, 0, 0);
    if (!fg) return;

    mActor_name_t cell = fg[s_cursor_x + s_cursor_z * GRID_MAX];

    char buf[FTR_BUF_SIZE];
    int oi = 0;

    if (cell == RSV_WALL_NO) {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "Wall");
    } else if (cell == EMPTY_NO) {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "Empty");
    } else if (cell == RSV_DOOR || cell == DOOR0 || cell == DOOR1 ||
               cell == EXIT_DOOR) {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "Door");
    } else if (ITEM_IS_FTR(cell)) {
        char name[FTR_NAME_SIZE];
        if (ftr_get_name(cell, name, sizeof(name)) > 0) {
            int size = mRmTp_ItemNo2FtrSize(cell);
            const char* size_str = "";
            if (size == mRmTp_FTRSIZE_1x1) size_str = "1 by 1";
            else if (size == mRmTp_FTRSIZE_1x2) size_str = "1 by 2";
            else if (size == mRmTp_FTRSIZE_2x2) size_str = "2 by 2";

            oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "%s, %s", name, size_str);
        } else {
            oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "Furniture");
        }
    } else {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "Position %d, %d", s_cursor_x, s_cursor_z);
    }

    /* Wall adjacency */
    char wall_buf[128];
    if (ftr_wall_adjacency(s_cursor_x, s_cursor_z, wall_buf, sizeof(wall_buf)) > 0) {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, ", adjacent to %s", wall_buf);
    }

    pc_acc_speak_interrupt(buf);
}

/* ========================================================================= */
/* Room overview                                                             */
/* ========================================================================= */

static void ftr_speak_room_overview(void) {
    if (!pc_acc_is_active()) return;

    mActor_name_t* fg = mFI_BkNum2UtFGTop_layer(0, 0, 0);
    if (!fg) return;

    int min_x, max_x, min_z, max_z;
    ftr_get_room_bounds(&min_x, &max_x, &min_z, &max_z);

    char buf[FTR_BUF_SIZE];
    int oi = 0;
    int count = 0;

    /* Track which furniture names we've already announced (multi-cell furniture) */
    mActor_name_t seen[64];
    int seen_x[64], seen_z[64];
    int seen_count = 0;

    /* Scan NW to SE */
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            mActor_name_t cell = fg[x + z * GRID_MAX];
            if (!ITEM_IS_FTR(cell)) continue;

            /* Skip if we already announced this item at an earlier position */
            int already_seen = 0;
            for (int s = 0; s < seen_count; s++) {
                if (seen[s] == cell && (seen_x[s] != x || seen_z[s] != z)) {
                    /* Same item ID at different position — multi-cell furniture.
                     * We report the first cell only. */
                    already_seen = 1;
                    break;
                }
            }

            /* For multi-cell items, we want to announce the top-left cell.
             * Check if this exact item ID already appeared at a position
             * that came earlier in the NW-to-SE scan. */
            if (already_seen) continue;

            if (seen_count < 64) {
                seen[seen_count] = cell;
                seen_x[seen_count] = x;
                seen_z[seen_count] = z;
                seen_count++;
            }

            char name[FTR_NAME_SIZE];
            if (ftr_get_name(cell, name, sizeof(name)) > 0) {
                if (oi > 0 && oi < FTR_BUF_SIZE - 2) {
                    oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, ". ");
                }
                oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "%s at %d, %d", name, x, z);
                count++;
            }
        }
    }

    if (count == 0) {
        pc_acc_speak_interrupt("Room is empty.");
    } else {
        char header[64];
        snprintf(header, sizeof(header), "%d items. ", count);

        char full[FTR_BUF_SIZE + 64];
        snprintf(full, sizeof(full), "%s%s", header, buf);
        pc_acc_speak_interrupt(full);
    }
}

/* ========================================================================= */
/* Wallpaper / carpet announcement                                           */
/* ========================================================================= */

static void ftr_speak_wallpaper_carpet(void) {
    if (!pc_acc_is_active()) return;

    int wall_idx = mRmTp_GetWallIdx();
    int floor_idx = mRmTp_GetFloorIdx();

    char buf[FTR_BUF_SIZE];
    int oi = 0;

    char name[FTR_NAME_SIZE];

    oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "Wallpaper: ");
    if (wall_idx >= 0 && ftr_get_decor_name(wall_idx, ITM_WALL_START, name, sizeof(name)) > 0) {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "%s", name);
    } else {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "unknown");
    }

    oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, ". Floor: ");
    if (floor_idx >= 0 && ftr_get_decor_name(floor_idx, ITM_CARPET_START, name, sizeof(name)) > 0) {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "%s", name);
    } else {
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "unknown");
    }

    pc_acc_speak_interrupt(buf);
}

/* ========================================================================= */
/* Push/pull/rotate narration (called from game hooks)                       */
/* ========================================================================= */

/* Convert aMR_DIRECT_* to compass name.
 * aMR_DIRECT_UP=0 is +Z (south in game), aMR_DIRECT_DOWN=2 is -Z (north).
 * Pull direction moves furniture toward player, push moves away. */
static const char* ftr_direct_name(int direct) {
    switch (direct) {
        case 0: return "north";  /* aMR_DIRECT_UP */
        case 1: return "west";   /* aMR_DIRECT_LEFT */
        case 2: return "south";  /* aMR_DIRECT_DOWN */
        case 3: return "east";   /* aMR_DIRECT_RIGHT */
        default: return "";
    }
}

void pc_acc_furniture_moved(u16 ftr_name, u8 shape, int direction, int ut_x, int ut_z, int layer) {
    if (!pc_acc_is_active()) return;

    char buf[FTR_BUF_SIZE];
    int oi = 0;

    char name[FTR_NAME_SIZE];
    ftr_get_name_from_idx(ftr_name, name, sizeof(name));

    const char* dir = ftr_direct_name(direction);

    if (s_enabled) {
        /* Enhanced mode: name, direction, position, adjacency */
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "%s, moving %s",
                       name[0] ? name : "Furniture", dir);

        /* Wall adjacency at new position */
        char wall_buf[128];
        if (ftr_wall_adjacency(ut_x, ut_z, wall_buf, sizeof(wall_buf)) > 0) {
            oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, ". Adjacent to %s", wall_buf);
        }

        /* Adjacent furniture */
        char adj_buf[256];
        if (ftr_adjacent_furniture(ut_x, ut_z, adj_buf, sizeof(adj_buf)) > 0) {
            oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, ". %s", adj_buf);
        }
    } else {
        /* Basic mode: name and direction */
        oi += snprintf(buf + oi, FTR_BUF_SIZE - oi, "%s, moving %s",
                       name[0] ? name : "Furniture", dir);
    }

    pc_acc_speak_interrupt(buf);
}

void pc_acc_furniture_blocked(int direction, u16 ftr_name, int target_ut_x, int target_ut_z) {
    if (!pc_acc_is_active()) return;

    mActor_name_t* fg = mFI_BkNum2UtFGTop_layer(0, 0, 0);

    char buf[FTR_BUF_SIZE];

    /* Check what's at the blocked position */
    if (fg && target_ut_x >= 0 && target_ut_x < GRID_MAX &&
        target_ut_z >= 0 && target_ut_z < GRID_MAX) {
        mActor_name_t blocker = fg[target_ut_x + target_ut_z * GRID_MAX];

        if (blocker == RSV_WALL_NO) {
            static const char* dir_wall[] = { "north", "west", "south", "east" };
            const char* wall_name = (direction >= 0 && direction < 4) ? dir_wall[direction] : "";
            snprintf(buf, sizeof(buf), "Blocked by %s wall", wall_name);
        } else if (ITEM_IS_FTR(blocker)) {
            char name[FTR_NAME_SIZE];
            if (ftr_get_name(blocker, name, sizeof(name)) > 0) {
                snprintf(buf, sizeof(buf), "Blocked by %s", name);
            } else {
                snprintf(buf, sizeof(buf), "Blocked by furniture");
            }
        } else {
            snprintf(buf, sizeof(buf), "Blocked");
        }
    } else {
        snprintf(buf, sizeof(buf), "Blocked");
    }

    pc_acc_speak_interrupt(buf);
}

void pc_acc_furniture_rotated(u16 ftr_name, u8 new_shape) {
    if (!pc_acc_is_active()) return;

    char name[FTR_NAME_SIZE];
    ftr_get_name_from_idx(ftr_name, name, sizeof(name));

    const char* facing = ftr_shape_facing_str(new_shape);
    char buf[128];

    if (facing) {
        snprintf(buf, sizeof(buf), "%s now facing %s",
                 name[0] ? name : "Furniture", facing);
    } else {
        snprintf(buf, sizeof(buf), "%s rotated",
                 name[0] ? name : "Furniture");
    }

    pc_acc_speak_interrupt(buf);
}

/* ========================================================================= */
/* Query whether Furniture Mode cursor is active (for input suppression)     */
/* ========================================================================= */

int pc_acc_furniture_mode_active(void) {
    return s_enabled;
}

/* ========================================================================= */
/* Main update — called once per frame                                       */
/* ========================================================================= */

void pc_acc_furniture_update(void) {
    if (!pc_acc_is_active()) return;

    const u8* keys = SDL_GetKeyboardState(NULL);
    if (!keys) return;

    u8 shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    u8 f_key = keys[SDL_SCANCODE_F];
    u8 r_key = keys[SDL_SCANCODE_R];

    /* Shift+F: wallpaper / carpet readout (works whether mode is on or off) */
    if (shift && f_key && !s_prev_f) {
        if (ftr_is_indoor_scene()) {
            ftr_speak_wallpaper_carpet();
        } else {
            pc_acc_speak_interrupt("Not indoors");
        }
    }
    /* F (no shift): toggle Furniture Mode */
    else if (!shift && f_key && !s_prev_f) {
        if (!ftr_is_indoor_scene()) {
            pc_acc_speak_interrupt("Furniture Mode only works indoors");
        } else {
            s_enabled = !s_enabled;
            if (s_enabled) {
                /* Initialize cursor to first valid position */
                int min_x, max_x, min_z, max_z;
                ftr_get_room_bounds(&min_x, &max_x, &min_z, &max_z);
                s_cursor_x = min_x;
                s_cursor_z = min_z;

                pc_acc_speak_interrupt("Furniture Mode on");
            } else {
                pc_acc_speak_interrupt("Furniture Mode off");
            }
        }
    }
    s_prev_f = f_key;

    /* R — Room overview (works indoors whether Furniture Mode is on or off) */
    if (r_key && !s_prev_r) {
        if (ftr_is_indoor_scene()) {
            ftr_speak_room_overview();
        } else {
            pc_acc_speak_interrupt("Not indoors");
        }
    }
    s_prev_r = r_key;

    if (!s_enabled) return;

    /* Auto-disable if player leaves the room */
    if (!ftr_is_indoor_scene()) {
        s_enabled = 0;
        return;
    }

    /* Get dynamic room bounds */
    int min_x, max_x, min_z, max_z;
    ftr_get_room_bounds(&min_x, &max_x, &min_z, &max_z);

    /* Arrow keys — virtual cursor movement */
    u8 up = keys[SDL_SCANCODE_UP];
    u8 down = keys[SDL_SCANCODE_DOWN];
    u8 left = keys[SDL_SCANCODE_LEFT];
    u8 right = keys[SDL_SCANCODE_RIGHT];

    int moved = 0;

    if (up && !s_prev_up) {
        if (s_cursor_z > min_z) { s_cursor_z--; moved = 1; }
    }
    if (down && !s_prev_down) {
        if (s_cursor_z < max_z) { s_cursor_z++; moved = 1; }
    }
    if (left && !s_prev_left) {
        if (s_cursor_x > min_x) { s_cursor_x--; moved = 1; }
    }
    if (right && !s_prev_right) {
        if (s_cursor_x < max_x) { s_cursor_x++; moved = 1; }
    }

    s_prev_up = up;
    s_prev_down = down;
    s_prev_left = left;
    s_prev_right = right;

    if (moved) {
        ftr_speak_cursor_cell();
    }
}

#else
/* Non-Windows stubs */
void pc_acc_furniture_update(void) {}
int pc_acc_furniture_mode_active(void) { return 0; }
void pc_acc_furniture_moved(u16 a, u8 b, int c, int d, int e, int f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
}
void pc_acc_furniture_blocked(int a, u16 b, int c, int d) {
    (void)a; (void)b; (void)c; (void)d;
}
void pc_acc_furniture_rotated(u16 a, u8 b) {
    (void)a; (void)b;
}
#endif
