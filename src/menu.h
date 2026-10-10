#ifndef LSD_MENU_H
#define LSD_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

// The settings menu (src/menu.cpp): F1, or a gamepad's Guide button or both
// its sticks pressed in, or the title menu's SETTINGS, opens it over the
// game. Call after SetUpSettings and SetUpControls, before the game starts.
// discPath is the game's .cue, which the menu's font is read from
// (ETC\FONTICON.TIM, the title menu's).
void SetUpMenu(const char* discPath);

// Opens the menu (the title menu's SETTINGS entry, src/title_settings.c).
void Menu_Open(void);

// Whether the menu is open.
int Menu_IsOpen(void);

#ifdef __cplusplus
}
#endif

#endif
