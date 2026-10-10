// The keyboard controls: controls.ini in the saves folder, written with the
// defaults when it is missing, read at startup and handed to psyz as pad 1's
// keyboard map (Psyz_PadsSetKeyboardMap). Gamepads keep SDL's mapping.
//
// The file is lines of `name = value`; `#` starts a comment.
//   layout = modern | classic     the keys to start from (default modern)
//   <button> = key, key ...       replaces one button's keys (none: unbound)
// Buttons are the PlayStation's: up down left right cross circle square
// triangle l1 r1 l2 r2 select start l3 r3. Keys are SDL's key names (SDL_
// GetScancodeName: "W", "Left Shift", "Space", "Up", "Escape", ...), which
// stand for a key's place on a US keyboard, not the letter printed on it.
//
// The settings menu (src/menu.cpp) changes the keys while the game runs and
// writes what it changed back into the file (Ini_Update): the layout line,
// and a line for each button whose keys are no longer the layout's.

#include "controls.h"
#include "ini.h"

#include <psyz.h>
#include <libetc.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>

#define MAX_KEYS_PER_BUTTON CONTROLS_KEYS_MAX

typedef struct {
    const char* name;
    unsigned short mask;
} Button;

// In the order the file lists them.
static const Button sButtons[] = {
    {"up", PADLup},       {"down", PADLdown},     {"left", PADLleft}, {"right", PADLright},
    {"cross", PADRdown},  {"circle", PADRright},  {"square", PADRleft},
    {"triangle", PADRup}, {"l1", PADL1},          {"r1", PADR1},      {"l2", PADL2},
    {"r2", PADR2},        {"select", PADselect},  {"start", PADstart}, {"l3", PADi},
    {"r3", PADj},
};
#define BUTTON_COUNT (int)(sizeof(sButtons) / sizeof(*sButtons))

typedef SDL_Scancode Layout[BUTTON_COUNT][MAX_KEYS_PER_BUTTON];

// The console's controls on a keyboard: psyz's built-in map.
static const Layout sClassic = {
    {SDL_SCANCODE_UP},     {SDL_SCANCODE_DOWN}, {SDL_SCANCODE_LEFT},      {SDL_SCANCODE_RIGHT},
    {SDL_SCANCODE_X},      {SDL_SCANCODE_D},    {SDL_SCANCODE_Z},         {SDL_SCANCODE_S},
    {SDL_SCANCODE_Q},      {SDL_SCANCODE_R},    {SDL_SCANCODE_W},         {SDL_SCANCODE_E},
    {SDL_SCANCODE_BACKSPACE}, {SDL_SCANCODE_RETURN}, {SDL_SCANCODE_1},    {SDL_SCANCODE_2},
};

// WASD or the arrows to walk and turn (the d-pad, which also moves menu
// cursors), Q/E to strafe (L2/R2), Shift to run (cross, held with forward),
// R/F to look up/down (triangle, square), Z/C to glance left/right (L1/R1),
// Space or Enter for circle (the link button, and confirm in menus),
// Backspace for cross (back in menus), Tab SELECT, Escape START (the pause;
// it no longer quits).
static const Layout sModern = {
    {SDL_SCANCODE_W, SDL_SCANCODE_UP},
    {SDL_SCANCODE_S, SDL_SCANCODE_DOWN},
    {SDL_SCANCODE_A, SDL_SCANCODE_LEFT},
    {SDL_SCANCODE_D, SDL_SCANCODE_RIGHT},
    {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_BACKSPACE},
    {SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN},
    {SDL_SCANCODE_F},
    {SDL_SCANCODE_R},
    {SDL_SCANCODE_Z},
    {SDL_SCANCODE_C},
    {SDL_SCANCODE_Q},
    {SDL_SCANCODE_E},
    {SDL_SCANCODE_TAB},
    {SDL_SCANCODE_ESCAPE},
    {0},
    {0},
};

static const struct {
    const char* name;
    const Layout* keys;
} sLayouts[] = {
    {"modern", &sModern},
    {"classic", &sClassic},
};
#define LAYOUT_COUNT (int)(sizeof(sLayouts) / sizeof(*sLayouts))

static const char sHeader[] =
    "# LSD: Dream Emulator keyboard controls (gamepads use SDL's own mapping).\n"
    "#\n"
    "# layout: modern (WASD or the arrows to move) or classic (the console's\n"
    "# buttons spread over the keyboard, arrows to move). To change a button,\n"
    "# remove the # before its line and list its keys, separated by commas (an\n"
    "# empty list unbinds it). Key names are SDL's, as in\n"
    "# https://wiki.libsdl.org/SDL3/SDL_Scancode without SDL_SCANCODE_: W, Space,\n"
    "# Left Shift, Return, Escape, Up, ... Delete this file to get the\n"
    "# defaults back.\n"
    "#\n"
    "# In LSD circle confirms in menus and cross goes back; in a dream the d-pad\n"
    "# walks and turns, cross held with forward runs, L2/R2 strafe, triangle and\n"
    "# square look up and down, L1/R1 glance left and right, circle is the link\n"
    "# button, START pauses, and SELECT held with triangle ends the dream.\n"
    "\n";

static void WriteDefaults(const char* path) {
    SDL_IOStream* io = SDL_IOFromFile(path, "w");
    if (io == NULL) {
        fprintf(stderr, "lsd: cannot write %s: %s\n", path, SDL_GetError());
        return;
    }
    SDL_IOprintf(io, "%slayout = %s\n\n# The %s layout's keys:\n", sHeader, sLayouts[0].name,
                 sLayouts[0].name);
    for (int b = 0; b < BUTTON_COUNT; b++) {
        SDL_IOprintf(io, "# %s =", sButtons[b].name);
        for (int k = 0; k < MAX_KEYS_PER_BUTTON && (*sLayouts[0].keys)[b][k] != 0; k++) {
            SDL_IOprintf(io, "%s %s", k ? "," : "", SDL_GetScancodeName((*sLayouts[0].keys)[b][k]));
        }
        SDL_IOprintf(io, "\n");
    }
    SDL_CloseIO(io);
}

// The keys in use, the layout they started from, and as controls.ini has
// them: the layout it names, its keys, and the buttons it lists.
static Layout sKeys;
static int sLayout;
static Layout sFileKeys;
static int sFileLayout;
static int sFileLists[BUTTON_COUNT];
static char* sPath; // controls.ini

// Applies one setting to sFileKeys (IniApplyFn).
static int ApplySetting(void* ctx, const char* name, char* value, const char* path, int lineNo) {
    Layout* keys = ctx;
    if (SDL_strcasecmp(name, "layout") == 0) {
        for (int i = 0; i < LAYOUT_COUNT; i++) {
            if (SDL_strcasecmp(value, sLayouts[i].name) == 0) {
                SDL_memcpy(keys, sLayouts[i].keys, sizeof(*keys));
                sFileLayout = i;
                return 0;
            }
        }
        fprintf(stderr, "lsd: %s:%d: no layout %s (modern, classic)\n", path, lineNo, value);
        return -1;
    }
    for (int b = 0; b < BUTTON_COUNT; b++) {
        if (SDL_strcasecmp(name, sButtons[b].name) != 0) {
            continue;
        }
        SDL_Scancode list[MAX_KEYS_PER_BUTTON] = {0};
        int n = 0;
        char* save = NULL;
        for (char* tok = SDL_strtok_r(value, ",", &save); tok != NULL;
             tok = SDL_strtok_r(NULL, ",", &save)) {
            tok = Ini_Trim(tok);
            if (*tok == '\0') {
                continue;
            }
            SDL_Scancode key = SDL_GetScancodeFromName(tok);
            if (key == SDL_SCANCODE_UNKNOWN) {
                fprintf(stderr, "lsd: %s:%d: no key named \"%s\"\n", path, lineNo, tok);
                return -1;
            }
            if (n == MAX_KEYS_PER_BUTTON) {
                fprintf(stderr, "lsd: %s:%d: at most %d keys per button\n", path, lineNo,
                        MAX_KEYS_PER_BUTTON);
                return -1;
            }
            list[n++] = key;
        }
        SDL_memcpy((*keys)[b], list, sizeof(list));
        sFileLists[b] = 1;
        return 0;
    }
    fprintf(stderr, "lsd: %s:%d: no button %s\n", path, lineNo, name);
    return -1;
}

// Hands sKeys to psyz as pad 1's keyboard map.
static void UseKeys(void) {
    PsyzKeyBinding map[BUTTON_COUNT * MAX_KEYS_PER_BUTTON];
    int count = 0;
    for (int b = 0; b < BUTTON_COUNT; b++) {
        for (int k = 0; k < MAX_KEYS_PER_BUTTON && sKeys[b][k] != 0; k++) {
            map[count].key = sKeys[b][k];
            map[count].buttons = sButtons[b].mask;
            count++;
        }
    }
    if (Psyz_PadsSetKeyboardMap(map, count) < 0) {
        fprintf(stderr, "lsd: the keyboard controls from %s were refused\n", sPath);
    }
}

void SetUpControls(const char* savesDir) {
    SDL_asprintf(&sPath, "%scontrols.ini", savesDir);
    SDL_memcpy(&sFileKeys, sLayouts[0].keys, sizeof(sFileKeys));

    // A line that means nothing is reported and skipped; the rest apply.
    if (Ini_Read(sPath, ApplySetting, &sFileKeys) != 0) {
        WriteDefaults(sPath);
    }
    SDL_memcpy(&sKeys, &sFileKeys, sizeof(sKeys));
    sLayout = sFileLayout;
    UseKeys();
}

int Controls_ButtonCount(void) {
    return BUTTON_COUNT;
}

const char* Controls_ButtonName(int b) {
    return sButtons[b].name;
}

int Controls_Keys(int b, int keys[CONTROLS_KEYS_MAX]) {
    int n = 0;
    while (n < MAX_KEYS_PER_BUTTON && sKeys[b][n] != 0) {
        keys[n] = sKeys[b][n];
        n++;
    }
    return n;
}

void Controls_SetKeys(int b, const int* keys, int count) {
    SDL_memset(sKeys[b], 0, sizeof(sKeys[b]));
    for (int k = 0; k < count && k < MAX_KEYS_PER_BUTTON; k++) {
        sKeys[b][k] = (SDL_Scancode)keys[k];
    }
    UseKeys();
}

int Controls_LayoutCount(void) {
    return LAYOUT_COUNT;
}

const char* Controls_LayoutName(int i) {
    return sLayouts[i].name;
}

int Controls_Layout(void) {
    return sLayout;
}

void Controls_SetLayout(int i) {
    sLayout = i;
    SDL_memcpy(&sKeys, sLayouts[i].keys, sizeof(sKeys));
    UseKeys();
}

// A button's keys as controls.ini lists them ("W, Up"), into out.
static void FormatKeys(const SDL_Scancode* keys, char* out, size_t size) {
    out[0] = '\0';
    for (int k = 0; k < MAX_KEYS_PER_BUTTON && keys[k] != 0; k++) {
        SDL_strlcat(out, k ? ", " : "", size);
        SDL_strlcat(out, SDL_GetScancodeName(keys[k]), size);
    }
}

int Controls_Save(void) {
    IniSetting set[1 + BUTTON_COUNT];
    char lists[BUTTON_COUNT][128];
    const Layout* layout = sLayouts[sLayout].keys;
    int count = 0;
    if (sLayout != sFileLayout) {
        set[count].name = "layout";
        set[count].value = sLayouts[sLayout].name;
        count++;
    }
    // A button the file lists, when its keys changed; one it doesn't, when
    // they are no longer the layout's.
    for (int b = 0; b < BUTTON_COUNT; b++) {
        const SDL_Scancode* than = sFileLists[b] ? sFileKeys[b] : (*layout)[b];
        if (SDL_memcmp(sKeys[b], than, sizeof(sKeys[b])) == 0) {
            continue;
        }
        FormatKeys(sKeys[b], lists[b], sizeof(lists[b]));
        set[count].name = sButtons[b].name;
        set[count].value = lists[b];
        count++;
        sFileLists[b] = 1;
    }
    if (count == 0) {
        return 0;
    }
    if (Ini_Update(sPath, set, count) != 0) {
        return -1;
    }
    SDL_memcpy(&sFileKeys, &sKeys, sizeof(sFileKeys));
    sFileLayout = sLayout;
    return 0;
}
