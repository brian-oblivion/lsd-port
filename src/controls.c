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

#include "controls.h"

#include <psyz.h>
#include <libetc.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>
#include <string.h>

#define MAX_KEYS_PER_BUTTON 4

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

static char* Trim(char* s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        *--end = '\0';
    }
    return s;
}

// Applies one line to `keys`. Returns 0, or -1 (and says why) when it means
// nothing.
static int ApplyLine(Layout* keys, char* line, const char* path, int lineNo) {
    char* hash = strchr(line, '#');
    if (hash != NULL) {
        *hash = '\0';
    }
    line = Trim(line);
    if (*line == '\0') {
        return 0;
    }
    char* eq = strchr(line, '=');
    if (eq == NULL) {
        fprintf(stderr, "lsd: %s:%d: expected name = value\n", path, lineNo);
        return -1;
    }
    *eq = '\0';
    char* name = Trim(line);
    char* value = Trim(eq + 1);

    if (SDL_strcasecmp(name, "layout") == 0) {
        for (int i = 0; i < LAYOUT_COUNT; i++) {
            if (SDL_strcasecmp(value, sLayouts[i].name) == 0) {
                SDL_memcpy(keys, sLayouts[i].keys, sizeof(*keys));
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
            tok = Trim(tok);
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
        return 0;
    }
    fprintf(stderr, "lsd: %s:%d: no button %s\n", path, lineNo, name);
    return -1;
}

void SetUpControls(const char* savesDir) {
    char* path;
    SDL_asprintf(&path, "%scontrols.ini", savesDir);
    Layout keys;
    SDL_memcpy(&keys, sLayouts[0].keys, sizeof(keys));

    size_t size;
    char* text = SDL_LoadFile(path, &size);
    if (text == NULL) {
        WriteDefaults(path);
    } else {
        // A line that means nothing is reported and skipped; the rest apply.
        int lineNo = 0;
        char* save = NULL;
        for (char* line = text; line != NULL; line = save) {
            save = strchr(line, '\n');
            if (save != NULL) {
                *save++ = '\0';
            }
            ApplyLine(&keys, line, path, ++lineNo);
        }
        SDL_free(text);
    }

    PsyzKeyBinding map[BUTTON_COUNT * MAX_KEYS_PER_BUTTON];
    int count = 0;
    for (int b = 0; b < BUTTON_COUNT; b++) {
        for (int k = 0; k < MAX_KEYS_PER_BUTTON && keys[b][k] != 0; k++) {
            map[count].key = keys[b][k];
            map[count].buttons = sButtons[b].mask;
            count++;
        }
    }
    if (Psyz_PadsSetKeyboardMap(map, count) < 0) {
        fprintf(stderr, "lsd: the keyboard controls from %s were refused\n", path);
    }
    SDL_free(path);
}
