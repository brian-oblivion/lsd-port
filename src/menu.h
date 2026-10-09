#ifndef LSD_MENU_H
#define LSD_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

// The settings menu (src/menu.cpp): F1, or a gamepad's Guide button or both
// its sticks pressed in, opens it over the game. Call after SetUpSettings and
// SetUpControls, before the game starts.
void SetUpMenu(void);

#ifdef __cplusplus
}
#endif

#endif
