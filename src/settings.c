// The settings in settings.ini (in the saves folder, beside controls.ini),
// written with the defaults on first start:
//   aspect = 4:3            the dream's width:height; wider shows more of it
//   resolution = 1          the 3D drawn at N times 320x240, 1 to 8
//   scale = sharp           nearest | sharp | smooth | integer
//   pace = 20               the dream's ticks a second, 10 to 30
//   smooth = off            frames drawn between the dream's ticks
// The command line (--aspect, --resolution, --scale, --pace, --smooth) and
// the environment (LSD_ASPECT, LSD_RESOLUTION, LSD_SCALE, LSD_PACE,
// LSD_SMOOTH) win over the file.

#include "settings.h"
#include "ini.h"
#include "pacing.h"
#include "widescreen.h"

#include <psyz.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float aspect;
    int resolution;
    PsyzScaleMode scale;
    int pace;
    int smooth;
} Settings;

static const Settings sDefaults = {4.0f / 3.0f, 1, PSYZ_SCALE_SHARP, PACING_PACE_DEFAULT, 0};

static const struct {
    const char* name;
    PsyzScaleMode mode;
} sScales[] = {
    {"nearest", PSYZ_SCALE_NEAREST},
    {"sharp", PSYZ_SCALE_SHARP},
    {"smooth", PSYZ_SCALE_SMOOTH},
    {"integer", PSYZ_SCALE_INTEGER},
};
#define SCALE_COUNT (int)(sizeof(sScales) / sizeof(*sScales))

static const char sDefaultFile[] =
    "# LSD: Dream Emulator settings. The command line (--aspect, --resolution,\n"
    "# --scale, --pace, --smooth) and the environment (LSD_ASPECT,\n"
    "# LSD_RESOLUTION, LSD_SCALE, LSD_PACE, LSD_SMOOTH) win over this file.\n"
    "# Delete it to get the defaults back.\n"
    "\n"
    "# The dream's width:height. 4:3 is the console's picture; a wider one,\n"
    "# such as 16:9, shows more of the dream at the sides. Menus and movies\n"
    "# stay 4:3.\n"
    "aspect = 4:3\n"
    "\n"
    "# The 3D drawn at this many times 320x240, 1 (the console's) to 8.\n"
    "resolution = 1\n"
    "\n"
    "# How the picture is scaled to the window:\n"
    "#   sharp    crisp, even pixels at any window size\n"
    "#   nearest  crisp, but some pixels a row or column wider than others\n"
    "#   smooth   blurred\n"
    "#   integer  whole multiples only, with a border\n"
    "scale = sharp\n"
    "\n"
    "# The dream's ticks a second, 10 to 30. 20 is what the game asks for;\n"
    "# a PlayStation managed about 14, so a dream lasted longer there.\n"
    "pace = 20\n"
    "\n"
    "# on: frames drawn between the dream's ticks, at the display's rate,\n"
    "# with the camera moving smoothly. off: each tick shown as it is.\n"
    "smooth = off\n";

static int ParseResolution(const char* s, int* out) {
    char* end;
    long n = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || n < 1 || n > PSYZ_INTERNAL_RES_MAX) {
        return -1;
    }
    *out = (int)n;
    return 0;
}

static int ParseScale(const char* s, PsyzScaleMode* out) {
    for (int i = 0; i < SCALE_COUNT; i++) {
        if (SDL_strcasecmp(s, sScales[i].name) == 0) {
            *out = sScales[i].mode;
            return 0;
        }
    }
    return -1;
}

static int ParsePace(const char* s, int* out) {
    char* end;
    long n = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || n < PACING_PACE_MIN || n > PACING_PACE_MAX) {
        return -1;
    }
    *out = (int)n;
    return 0;
}

static int ParseOnOff(const char* s, int* out) {
    if (SDL_strcasecmp(s, "on") == 0 || SDL_strcmp(s, "1") == 0) {
        *out = 1;
    } else if (SDL_strcasecmp(s, "off") == 0 || SDL_strcmp(s, "0") == 0) {
        *out = 0;
    } else {
        return -1;
    }
    return 0;
}

// IniApplyFn for settings.ini, into the Settings ctx.
static int ApplySetting(void* ctx, const char* name, char* value, const char* path, int lineNo) {
    Settings* set = ctx;
    if (SDL_strcasecmp(name, "aspect") == 0) {
        if (Widescreen_Parse(value, &set->aspect) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: aspect wants width:height, such as 16:9\n", path, lineNo);
    } else if (SDL_strcasecmp(name, "resolution") == 0) {
        if (ParseResolution(value, &set->resolution) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: resolution wants 1 to %d\n", path, lineNo,
                PSYZ_INTERNAL_RES_MAX);
    } else if (SDL_strcasecmp(name, "scale") == 0) {
        if (ParseScale(value, &set->scale) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: scale wants nearest, sharp, smooth or integer\n", path,
                lineNo);
    } else if (SDL_strcasecmp(name, "pace") == 0) {
        if (ParsePace(value, &set->pace) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: pace wants %d to %d\n", path, lineNo, PACING_PACE_MIN,
                PACING_PACE_MAX);
    } else if (SDL_strcasecmp(name, "smooth") == 0) {
        if (ParseOnOff(value, &set->smooth) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: smooth wants on or off\n", path, lineNo);
    } else {
        fprintf(stderr, "lsd: %s:%d: no setting %s\n", path, lineNo, name);
    }
    return -1;
}

static void WriteDefaults(const char* path) {
    SDL_IOStream* io = SDL_IOFromFile(path, "w");
    if (io == NULL) {
        fprintf(stderr, "lsd: cannot write %s: %s\n", path, SDL_GetError());
        return;
    }
    SDL_WriteIO(io, sDefaultFile, sizeof(sDefaultFile) - 1);
    SDL_CloseIO(io);
}

int SetUpSettings(const char* savesDir, const SettingArgs* args,
                  void (*error)(const char* fmt, ...)) {
    Settings set = sDefaults;
    char* path;
    SDL_asprintf(&path, "%ssettings.ini", savesDir);
    if (Ini_Read(path, ApplySetting, &set) != 0) {
        WriteDefaults(path);
    }
    SDL_free(path);

    if (args->aspect != NULL && Widescreen_Parse(args->aspect, &set.aspect) != 0) {
        error("--aspect wants width:height, such as 16:9 (got %s)", args->aspect);
        return -1;
    }
    if (args->resolution != NULL && ParseResolution(args->resolution, &set.resolution) != 0) {
        error("--resolution wants 1 to %d (got %s)", PSYZ_INTERNAL_RES_MAX, args->resolution);
        return -1;
    }
    if (args->scale != NULL && ParseScale(args->scale, &set.scale) != 0) {
        error("--scale wants nearest, sharp, smooth or integer (got %s)", args->scale);
        return -1;
    }
    if (args->pace != NULL && ParsePace(args->pace, &set.pace) != 0) {
        error("--pace wants %d to %d (got %s)", PACING_PACE_MIN, PACING_PACE_MAX, args->pace);
        return -1;
    }
    if (args->smooth != NULL && ParseOnOff(args->smooth, &set.smooth) != 0) {
        error("LSD_SMOOTH wants on or off (got %s)", args->smooth);
        return -1;
    }

    Widescreen_Init(set.aspect);
    Psyz_VideoSetInternalResolution((unsigned)set.resolution);
    Psyz_VideoSetScaleMode(set.scale);
    Pacing_Init(set.pace, set.smooth);
    return 0;
}
