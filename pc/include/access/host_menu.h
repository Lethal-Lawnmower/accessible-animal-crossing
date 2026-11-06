/* host_menu.h - Speech for the port's own RmlUi menus (prelaunch, settings,
 * controller setup, the in-game menu bar opened with Escape).
 *
 * Works generically from RmlUi focus: whatever control has focus is read as
 * its visible text (label, then value). When the focused control's value
 * changes, only the new value is spoken. */
#ifndef ACCESS_HOST_MENU_H
#define ACCESS_HOST_MENU_H

/* Call once per frame after the UI has been updated. */
void acc_host_menu_update();

#endif /* ACCESS_HOST_MENU_H */
