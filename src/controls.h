#ifndef LSD_CONTROLS_H
#define LSD_CONTROLS_H

// Reads <savesDir>controls.ini (writing it with the defaults when missing)
// and sets pad 1's keyboard map from it. savesDir ends in a separator.
void SetUpControls(const char* savesDir);

// For the settings menu (src/menu.cpp), once SetUpControls has run.

// The most keys a button takes.
#define CONTROLS_KEYS_MAX 4

// The PlayStation's buttons, as controls.ini names them ("up", "cross").
int Controls_ButtonCount(void);
const char* Controls_ButtonName(int b);

// Button b's keys (SDL_Scancodes) into keys; returns how many.
int Controls_Keys(int b, int keys[CONTROLS_KEYS_MAX]);

// Gives button b these keys (at most CONTROLS_KEYS_MAX), at once.
void Controls_SetKeys(int b, const int* keys, int count);

// The layouts ("modern", "classic"), and the one the keys started from.
int Controls_LayoutCount(void);
const char* Controls_LayoutName(int i);
int Controls_Layout(void);

// Every button the layout's keys, at once.
void Controls_SetLayout(int i);

// Writes what changed since controls.ini was read (or last saved) into it,
// keeping its comments and other lines. Returns 0, or -1 when it cannot be
// written.
int Controls_Save(void);

#endif
