/* pc_pad.c - GC controller input via SDL gamepad + keyboard */
#include "pc_platform.h"
#include "pc_typing.h"
#include "pc_keybindings.h"
#include "pc_acc_nav.h"
#include "pc_acc_furniture.h"
#include "pc_acc_gameplay.h"
#include <dolphin/pad.h>

/* analog stick constants */
#define STICK_MAGNITUDE     80
#define AXIS_DEADZONE       4000
#define TRIGGER_THRESHOLD   100
#define RUMBLE_DURATION_MS  200

static SDL_GameController* g_controller = NULL;

BOOL PADInit(void) {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                break;
            }
        }
    }
    return TRUE;
}

u32 PADRead(PADStatus* status) {
    memset(status, 0, sizeof(PADStatus) * 4);

    const u8* keys = SDL_GetKeyboardState(NULL);
    u32 mouse = SDL_GetMouseState(NULL, NULL);
    u16 buttons = 0;
    s8 stickX = 0, stickY = 0;
    s8 cstickX = 0, cstickY = 0;

    /* Suppress keyboard-to-button mapping when typing into the in-game text editor */
    if (!(g_pc_typing_mode && g_pc_editor_active)) {
        /* helper: check if a PCInputCode is currently pressed */
        #define INPUT_PRESSED(code) \
            (((code) & PC_INPUT_MOUSE_BIT) \
                ? (mouse & SDL_BUTTON((code) & 0xFF)) \
                : keys[(SDL_Scancode)(code)])

        /* buttons (from keybindings.ini) */
        PCKeybindings* kb = &g_pc_keybindings;
        if (INPUT_PRESSED(kb->a))     buttons |= PAD_BUTTON_A;
        if (INPUT_PRESSED(kb->b))     buttons |= PAD_BUTTON_B;
        if (INPUT_PRESSED(kb->x))     buttons |= PAD_BUTTON_X;
        if (INPUT_PRESSED(kb->y))     buttons |= PAD_BUTTON_Y;
        if (INPUT_PRESSED(kb->start)) buttons |= PAD_BUTTON_START;
        if (INPUT_PRESSED(kb->z))     buttons |= PAD_TRIGGER_Z;
        if (INPUT_PRESSED(kb->l))     buttons |= PAD_TRIGGER_L;
        if (INPUT_PRESSED(kb->r))     buttons |= PAD_TRIGGER_R;

        /* main stick */
        if (INPUT_PRESSED(kb->stick_up))    stickY += STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_down))  stickY -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_left))  stickX -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_right)) stickX += STICK_MAGNITUDE;

        /* C-stick */
        if (INPUT_PRESSED(kb->cstick_up))    cstickY += STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_down))  cstickY -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_left))  cstickX -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_right)) cstickX += STICK_MAGNITUDE;

        /* D-pad — suppress J/K/L when accessibility nav consumes them */
        {
            int nav_active = pc_acc_nav_is_active();
            int suppress_j = nav_active && kb->dpad_left  == SDL_SCANCODE_J;
            int suppress_k = nav_active && kb->dpad_down  == SDL_SCANCODE_K;
            int suppress_l = nav_active && kb->dpad_right == SDL_SCANCODE_L;

            if (INPUT_PRESSED(kb->dpad_up))                     buttons |= PAD_BUTTON_UP;
            if (INPUT_PRESSED(kb->dpad_down)  && !suppress_k)   buttons |= PAD_BUTTON_DOWN;
            if (INPUT_PRESSED(kb->dpad_left)  && !suppress_j)   buttons |= PAD_BUTTON_LEFT;
            if (INPUT_PRESSED(kb->dpad_right) && !suppress_l)   buttons |= PAD_BUTTON_RIGHT;
        }

        /* C-stick — suppress arrow keys when accessibility menus/modes consume them */
        if (pc_acc_furniture_mode_active()) {
            cstickX = 0;
            cstickY = 0;
        }

        #undef INPUT_PRESSED
    }

    /* hotplug */
    if (!g_controller) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                g_controller = SDL_GameControllerOpen(i);
                if (g_controller) break;
            }
        }
    }

    if (g_controller) {
        if (!SDL_GameControllerGetAttached(g_controller)) {
            SDL_GameControllerClose(g_controller);
            g_controller = NULL;
        }
    }
    if (g_controller) {
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_A)) buttons |= PAD_BUTTON_A;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_B)) buttons |= PAD_BUTTON_B;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_X)) buttons |= PAD_BUTTON_X;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_Y)) buttons |= PAD_BUTTON_Y;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START)) buttons |= PAD_BUTTON_START;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_BACK))  buttons |= PAD_BUTTON_START;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  buttons |= PAD_TRIGGER_L;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= PAD_TRIGGER_Z;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    buttons |= PAD_BUTTON_UP;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  buttons |= PAD_BUTTON_DOWN;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  buttons |= PAD_BUTTON_LEFT;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= PAD_BUTTON_RIGHT;

        s16 lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        s16 ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (abs(lx) > AXIS_DEADZONE) {
            int sx = lx >> 8;
            if (sx > 127) sx = 127; else if (sx < -128) sx = -128;
            stickX = (s8)sx;
        }
        if (abs(ly) > AXIS_DEADZONE) {
            int sy = -(ly >> 8);
            if (sy > 127) sy = 127; else if (sy < -128) sy = -128;
            stickY = (s8)sy;
        }

        s16 rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        s16 ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        if (abs(rx) > AXIS_DEADZONE) {
            int srx = rx >> 8;
            if (srx > 127) srx = 127; else if (srx < -128) srx = -128;
            cstickX = (s8)srx;
        }
        if (abs(ry) > AXIS_DEADZONE) {
            int sry = -(ry >> 8);
            if (sry > 127) sry = 127; else if (sry < -128) sry = -128;
            cstickY = (s8)sry;
        }

        u8 lt = (u8)(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
        u8 rt = (u8)(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);
        if (lt > TRIGGER_THRESHOLD) buttons |= PAD_TRIGGER_L;
        if (rt > TRIGGER_THRESHOLD) buttons |= PAD_TRIGGER_R;
        status[0].triggerLeft = lt;
        status[0].triggerRight = rt;
    }

    /* Auto-catch input injection: override stick and press A when auto-approach is active */
    {
        s8 auto_sx, auto_sy;
        int auto_a;
        pc_acc_gameplay_get_auto_input(&auto_sx, &auto_sy, &auto_a);
        if (auto_sx != 0 || auto_sy != 0) {
            stickX = auto_sx;
            stickY = auto_sy;
        }
        if (auto_a) {
            buttons |= PAD_BUTTON_A;
        }
    }

    status[0].button = buttons;
    status[0].stickX = stickX;
    status[0].stickY = stickY;
    status[0].substickX = cstickX;
    status[0].substickY = cstickY;
    status[0].err = 0; /* PAD_ERR_NONE */

    return PAD_CHAN0_BIT; /* Controller 1 connected */
}

void PADControlMotor(s32 chan, u32 command) {
    if (g_controller && chan == 0) {
        u16 intensity = (command == 1) ? 0xFFFF : 0;
        SDL_GameControllerRumble(g_controller, intensity, intensity, RUMBLE_DURATION_MS);
    }
}

void PADControlAllMotors(const u32* commands) {
    PADControlMotor(0, commands[0]);
}

void PADCleanup(void) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
}

BOOL PADReset(u32 mask) { (void)mask; return TRUE; }
BOOL PADRecalibrate(u32 mask) { (void)mask; return TRUE; }
BOOL PADSync(void) { return TRUE; }
void PADSetSpec(u32 spec) { (void)spec; }
void PADSetAnalogMode(u32 mode) { (void)mode; }
/* PADClamp compiled from decomp: src/static/dolphin/pad/Padclamp.c */
BOOL PADGetType(s32 chan, u32* type) { if (type) *type = 0x09000000; return TRUE; }
