/* pc_acc_hotkeys.h - Accessibility hotkeys and auto-announcements.
 *
 * Hotkeys: F1=time/weather, F2=bells, F3=location, F4=equipped item
 * Auto: scene changes, acre transitions when walking outdoors */
#ifndef PC_ACC_HOTKEYS_H
#define PC_ACC_HOTKEYS_H

/* Call once per frame to check for accessibility hotkey presses
 * and auto-announce scene/acre changes. */
void pc_acc_hotkeys_update(void);

#endif
