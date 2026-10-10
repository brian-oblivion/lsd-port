// The host's entry point: reads the port's own options, brings up the tools
// psyz offers, then runs the game's main (decomp/src/main.c, compiled with
// main renamed to lsd_game_main), which never returns.
//
// Exit codes: 2 when there is no usable disc image or saves folder; otherwise the game's
// (0 after --frames N).

#include "controls.h"
#include "menu.h"
#include "settings.h"
#include "title_settings.h"

#include <psyz.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_stdinc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

void lsd_game_main(void);

// Whether a box should say why lsd cannot start, as well as stderr: on
// Windows, where a game is usually started from Explorer and the console
// closes with it, and elsewhere when stderr is not a terminal (started from
// a desktop, a Steam library or Finder) unless SDL has no real display to
// put a box on (headless runs and CI).
static int StartErrorInBox(void) {
#ifdef _WIN32
    return 1;
#else
    const char* driver = SDL_getenv("SDL_VIDEO_DRIVER");
    return !isatty(STDERR_FILENO) &&
           (driver == NULL ||
            (strcmp(driver, "offscreen") != 0 && strcmp(driver, "dummy") != 0));
#endif
}

// Says why lsd cannot start, on stderr and, when StartErrorInBox, in a box.
static void StartError(const char* fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    fprintf(stderr, "lsd: %s\n", msg);
    if (StartErrorInBox()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LSD: Dream Emulator", msg, NULL);
    }
}

// --frames N: exit after N VSyncs, for smoke tests; 0 runs forever.
static int sFrameLimit;
static int sFrameCount;
static PsyzVSyncCb sNextVSyncCb;

static void CountFrame(void) {
    if (sNextVSyncCb != NULL) {
        sNextVSyncCb();
    }
    if (sFrameLimit > 0 && ++sFrameCount >= sFrameLimit) {
        exit(0);
    }
}

// --saves DIR (or LSD_SAVES): where the memory cards live, as bu00/ and
// bu10/ (psyz's "bu00:" and "bu10:"). By default SDL's per-user folder:
// ~/.local/share/lsd-dream-emulator/ on Linux, %APPDATA%\lsd-dream-emulator\ on Windows.
static char* sSavesDir;

// Psyz_AdjustPathCB: "buXY:NAME" becomes <saves>/buXY/NAME, and "buXY:" or
// "buXY:*" (a card's directory) <saves>/buXY/.
static int AdjustCardPath(char* dst, const char* src, int maxlen) {
    if (strncmp(src, "bu", 2) != 0 || strlen(src) < 5 || src[4] != ':') {
        return -1;
    }
    const char* name = src + 5;
    if (strcmp(name, "*") == 0) {
        name = "";
    }
    return SDL_snprintf(dst, maxlen, "%s%.4s/%s", sSavesDir, src, name);
}

// Picks the saves folder, creates it, and routes psyz's card paths there.
// Returns 0, or -1 when the folder cannot be made.
static int SetUpSaves(const char* dir) {
    if (dir != NULL && *dir != '\0') {
        size_t len = strlen(dir);
        int hasSep = len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\');
        SDL_asprintf(&sSavesDir, "%s%s", dir, hasSep ? "" : "/");
    } else {
        sSavesDir = SDL_GetPrefPath(NULL, "lsd-dream-emulator");
        if (sSavesDir == NULL) {
            StartError("no per-user folder for saves (%s); pass --saves DIR", SDL_GetError());
            return -1;
        }
    }
    if (!SDL_CreateDirectory(sSavesDir)) {
        StartError("cannot make the saves folder %s: %s", sSavesDir, SDL_GetError());
        return -1;
    }

    // Builds before this one kept the cards in the working directory. Say so
    // rather than moving them (README, "Saves").
    char* card;
    SDL_asprintf(&card, "%sbu00", sSavesDir);
    SDL_PathInfo info;
    if (SDL_GetPathInfo("bu00", &info) && info.type == SDL_PATHTYPE_DIRECTORY &&
        !SDL_GetPathInfo(card, NULL)) {
        fprintf(stderr,
                "lsd: bu00/ here holds memory cards from an older build; saves now go to %s"
                " (move bu00/ and bu10/ there to keep them)\n",
                sSavesDir);
    }
    SDL_free(card);

    Psyz_AdjustPathCB(AdjustCardPath);
    return 0;
}

// The only .cue in `dir`/disc/, as a path; NULL when there is none, or more
// than one to choose from (and then says so).
static char* FindDiscIn(const char* dir) {
    char* discDir;
    SDL_asprintf(&discDir, "%sdisc", dir);
    int count = 0;
    char** cues = SDL_GlobDirectory(discDir, "*.cue", SDL_GLOB_CASEINSENSITIVE, &count);
    char* path = NULL;
    if (cues != NULL && count == 1) {
        SDL_asprintf(&path, "%s/%s", discDir, cues[0]);
    } else if (count > 1) {
        StartError("%d .cue files in %s; pass --disc to pick one", count, discDir);
    }
    SDL_free(cues);
    SDL_free(discDir);
    return path;
}

// The folder a packaged lsd was started from, as the player sees it, with a
// trailing separator; NULL when lsd is not packaged. An AppImage runs lsd from
// a mount of its own, so it is the folder holding the .AppImage ($APPIMAGE);
// a macOS .app has SDL's base path inside the bundle (Contents/Resources/),
// so it is the folder holding the .app.
static char* PackageDir(void) {
    char* dir = NULL;
#if defined(__APPLE__)
    const char* base = SDL_GetBasePath();
    const char* app = base != NULL ? strstr(base, ".app/Contents/") : NULL;
    if (app != NULL) {
        dir = SDL_strndup(base, (size_t)(app - base));
    }
#elif !defined(_WIN32)
    const char* appimage = SDL_getenv("APPIMAGE");
    if (appimage != NULL && *appimage != '\0') {
        dir = SDL_strdup(appimage);
    }
#endif
    char* slash = dir != NULL ? strrchr(dir, '/') : NULL;
    if (slash == NULL) {
        SDL_free(dir);
        return NULL;
    }
    slash[1] = '\0';
    return dir;
}

// The disc image when neither --disc nor LSD_DISC names one: the only .cue
// in disc/ under the working directory, else in disc/ beside lsd itself,
// else beside the AppImage or the .app, else in the saves folder
// (README, "The game").
static char* FindDefaultDisc(void) {
    char* path = FindDiscIn("");
    const char* base = SDL_GetBasePath();
    if (path == NULL && base != NULL) {
        path = FindDiscIn(base);
    }
    char* package = PackageDir();
    if (path == NULL && package != NULL) {
        path = FindDiscIn(package);
    }
    SDL_free(package);
    if (path == NULL) {
        path = FindDiscIn(sSavesDir);
    }
    return path;
}

int main(int argc, char** argv) {
    // --disc FILE.cue (or LSD_DISC): the user's disc image, which psyz's
    // libcd reads. The game needs it from its first file on.
    const char* disc = getenv("LSD_DISC");
    const char* saves = getenv("LSD_SAVES");
    // --aspect W:H, --resolution N, --scale MODE, --pace N, --smooth on|off,
    // --frame-rate N|display, --draw-distance N, --dither on|off,
    // --colour console|full, --geometry console|precise|perspective (or
    // LSD_ASPECT, LSD_RESOLUTION, LSD_SCALE, LSD_PACE, LSD_SMOOTH,
    // LSD_FRAME_RATE, LSD_DRAW_DISTANCE, LSD_DITHER, LSD_COLOUR, LSD_GEOMETRY):
    // the picture and the dream's pacing, over settings.ini (src/settings.c).
    SettingArgs settings = {0};
    Settings_FromEnv(&settings);
    for (int i = 1; i < argc - 1; i++) {
        const char* value = argv[i + 1];
        if (strcmp(argv[i], "--frames") == 0) {
            sFrameLimit = atoi(value);
        } else if (strcmp(argv[i], "--disc") == 0) {
            disc = value;
        } else if (strcmp(argv[i], "--saves") == 0) {
            saves = value;
        } else {
            Settings_FromArg(&settings, argv[i], value);
        }
    }
    // The saves folder first: the disc image can be in it.
    if (SetUpSaves(saves) != 0) {
        return 2;
    }
    if (disc == NULL) {
        disc = FindDefaultDisc();
    }
    if (disc == NULL) {
        StartError("no disc image; put it in a folder disc/ beside lsd or in %s,"
                   " or pass --disc path/to/game.cue",
                   sSavesDir);
        return 2;
    }
    if (Psyz_CdSetDiskPath(disc) != 0) {
        StartError("cannot read the disc image %s", disc);
        return 2;
    }
    SetUpControls(sSavesDir);
    if (SetUpSettings(sSavesDir, &settings, StartError) != 0) {
        return 2;
    }
    // F1, or a gamepad's Guide button or both sticks pressed in, or the title
    // menu's SETTINGS: the settings menu over the game (src/menu.cpp,
    // src/title_settings.c).
    SetUpMenu(disc);
    TitleSettings_Init();
    if (sFrameLimit > 0) {
        sNextVSyncCb = Psyz_SetVSyncCb(CountFrame);
    }

    // LSD_DEBUG_PORT=<port>: psyz's debug server on 127.0.0.1 (screenshots,
    // VRAM, input injection) for tools and agents.
    const char* debugPort = getenv("LSD_DEBUG_PORT");
    if (debugPort != NULL) {
        int port = Psyz_DebugServer(atoi(debugPort));
        fprintf(stderr, "debug server on 127.0.0.1:%d\n", port);
    }

    // LSD_VSYNC=auto|on|off|limitless: psyz's frame pacing. "off" paces with
    // psyz's own limiter (59.94 fps), so the game keeps running while its
    // window is hidden or when there is no display.
    static const struct {
        const char* name;
        PsyzVsyncMode mode;
    } vsyncModes[] = {
        {"auto", PSYZ_VSYNC_AUTO},
        {"on", PSYZ_VSYNC_ON},
        {"off", PSYZ_VSYNC_OFF},
        {"limitless", PSYZ_VSYNC_LIMITLESS},
    };
    const char* vsync = getenv("LSD_VSYNC");
    for (size_t i = 0; vsync != NULL && i < sizeof(vsyncModes) / sizeof(*vsyncModes); i++) {
        if (strcmp(vsync, vsyncModes[i].name) == 0) {
            Psyz_VideoSetVsyncMode(vsyncModes[i].mode);
        }
    }

    lsd_game_main();
    return 0;
}
