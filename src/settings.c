// The picture settings in settings.ini (in the saves folder, beside
// controls.ini), written with the defaults on first start:
//   aspect = 4:3            the dream's width:height; wider shows more of it
//   resolution = 1          the 3D drawn at N times 320x240, 1 to 8
//   scale = sharp           nearest | sharp | smooth | integer
// The command line (--aspect, --resolution, --scale) and the environment
// (LSD_ASPECT, LSD_RESOLUTION, LSD_SCALE) win over the file.

#include "settings.h"
#include "ini.h"
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
} Picture;

static const Picture sDefaults = {4.0f / 3.0f, 1, PSYZ_SCALE_SHARP};

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
    "# LSD: Dream Emulator picture settings. The command line (--aspect,\n"
    "# --resolution, --scale) and the environment (LSD_ASPECT, LSD_RESOLUTION,\n"
    "# LSD_SCALE) win over this file. Delete it to get the defaults back.\n"
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
    "scale = sharp\n";

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

// IniApplyFn for settings.ini, into the Picture ctx.
static int ApplySetting(void* ctx, const char* name, char* value, const char* path, int lineNo) {
    Picture* pic = ctx;
    if (SDL_strcasecmp(name, "aspect") == 0) {
        if (Widescreen_Parse(value, &pic->aspect) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: aspect wants width:height, such as 16:9\n", path, lineNo);
    } else if (SDL_strcasecmp(name, "resolution") == 0) {
        if (ParseResolution(value, &pic->resolution) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: resolution wants 1 to %d\n", path, lineNo,
                PSYZ_INTERNAL_RES_MAX);
    } else if (SDL_strcasecmp(name, "scale") == 0) {
        if (ParseScale(value, &pic->scale) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: scale wants nearest, sharp, smooth or integer\n", path,
                lineNo);
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

int SetUpPicture(const char* savesDir, const char* aspect, const char* resolution,
                 const char* scale, void (*error)(const char* fmt, ...)) {
    Picture pic = sDefaults;
    char* path;
    SDL_asprintf(&path, "%ssettings.ini", savesDir);
    if (Ini_Read(path, ApplySetting, &pic) != 0) {
        WriteDefaults(path);
    }
    SDL_free(path);

    if (aspect != NULL && Widescreen_Parse(aspect, &pic.aspect) != 0) {
        error("--aspect wants width:height, such as 16:9 (got %s)", aspect);
        return -1;
    }
    if (resolution != NULL && ParseResolution(resolution, &pic.resolution) != 0) {
        error("--resolution wants 1 to %d (got %s)", PSYZ_INTERNAL_RES_MAX, resolution);
        return -1;
    }
    if (scale != NULL && ParseScale(scale, &pic.scale) != 0) {
        error("--scale wants nearest, sharp, smooth or integer (got %s)", scale);
        return -1;
    }

    Widescreen_Init(pic.aspect);
    Psyz_VideoSetInternalResolution((unsigned)pic.resolution);
    Psyz_VideoSetScaleMode(pic.scale);
    return 0;
}
