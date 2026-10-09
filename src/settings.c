// The settings in settings.ini (in the saves folder, beside controls.ini),
// written with the defaults on first start:
//   aspect = 4:3            the dream's width:height; wider shows more of it
//   resolution = 1          the 3D drawn at N times 320x240, 1 to 8
//   scale = sharp           nearest | sharp | smooth | integer
//   pace = 14               the dream's ticks a second, 10 to 30
//   smooth = on             frames drawn between the dream's ticks
//   frame_rate = 60         their rate: 60, display, or 30 to 360
//   draw_distance = 1       the dream's fog N times further away, 1 to 4
//   dither = on             the console's 4x4 dither pattern, or off
//   colour = console        console (15-bit) | full (24-bit, no dither)
//   geometry = console      console | precise | perspective
// The command line (--aspect, --resolution, --scale, --pace, --smooth,
// --frame-rate, --draw-distance, --dither, --colour, --geometry) and the
// environment (LSD_ASPECT, LSD_RESOLUTION, LSD_SCALE, LSD_PACE, LSD_SMOOTH,
// LSD_FRAME_RATE, LSD_DRAW_DISTANCE, LSD_DITHER, LSD_COLOUR, LSD_GEOMETRY)
// win over the file.

#include "settings.h"
#include "draw_distance.h"
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
    int frameRate;
    int drawDistance;
    int dither;
    PsyzColorDepth colour;
    PsyzGeometry geometry;
} Settings;

static const Settings sDefaults = {
    4.0f / 3.0f, 1, PSYZ_SCALE_SHARP, 14, 1, PACING_FRAME_RATE_CONSOLE, 1, 1,
    PSYZ_COLOR_DEPTH_15, PSYZ_GEOMETRY_CONSOLE,
};

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
    "# --scale, --pace, --smooth, --frame-rate, --draw-distance, --dither,\n"
    "# --colour, --geometry) and the environment (LSD_ASPECT, LSD_RESOLUTION,\n"
    "# LSD_SCALE, LSD_PACE, LSD_SMOOTH, LSD_FRAME_RATE, LSD_DRAW_DISTANCE,\n"
    "# LSD_DITHER, LSD_COLOUR, LSD_GEOMETRY) win over this file.\n"
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
    "# The dream's ticks a second, 10 to 30. 14 is about what a PlayStation\n"
    "# managed; 20 is what the game asks for, faster, and a dream ends sooner.\n"
    "pace = 14\n"
    "\n"
    "# on: frames drawn between the dream's ticks, at 59.94 a second, with\n"
    "# the camera moving smoothly. off: each tick shown as it is.\n"
    "smooth = on\n"
    "\n"
    "# With smooth on, the dream's frames a second: 60, the console's; display,\n"
    "# your display's refresh rate (120, 144, ...); or a number, 30 to 360.\n"
    "frame_rate = 60\n"
    "\n"
    "# How far the dream shows before it fades into the fog, 1 to 4: the fog\n"
    "# N times further away, never past the clearest a stage has. 1 is the\n"
    "# console's.\n"
    "draw_distance = 1\n"
    "\n"
    "# on: the console's 4x4 dither pattern over shading, which at a higher\n"
    "# resolution shows as grain. off: none, and the colour shows in bands.\n"
    "dither = on\n"
    "\n"
    "# console: colour rounded to the console's 15 bits. full: 24 bits, with\n"
    "# smooth shading and no dithering (whatever dither says).\n"
    "colour = console\n"
    "\n"
    "# console: the 3D's corners on whole pixels and its textures mapped flat,\n"
    "# so the ground wobbles as the view moves and textures bend near it.\n"
    "# precise: the corners where they fall between pixels. perspective:\n"
    "# precise, and the textures in perspective.\n"
    "geometry = console\n";

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

static int ParseDrawDistance(const char* s, int* out) {
    char* end;
    long n = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || n < DRAW_DISTANCE_MIN || n > DRAW_DISTANCE_MAX) {
        return -1;
    }
    *out = (int)n;
    return 0;
}

static int ParseFrameRate(const char* s, int* out) {
    char* end;
    long n;
    if (SDL_strcasecmp(s, "display") == 0) {
        *out = PACING_FRAME_RATE_DISPLAY;
        return 0;
    }
    n = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || n < PACING_FRAME_RATE_MIN || n > PACING_FRAME_RATE_MAX) {
        return -1;
    }
    *out = n == 60 ? PACING_FRAME_RATE_CONSOLE : (int)n;
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

static int ParseColour(const char* s, PsyzColorDepth* out) {
    if (SDL_strcasecmp(s, "console") == 0) {
        *out = PSYZ_COLOR_DEPTH_15;
    } else if (SDL_strcasecmp(s, "full") == 0) {
        *out = PSYZ_COLOR_DEPTH_24;
    } else {
        return -1;
    }
    return 0;
}

static int ParseGeometry(const char* s, PsyzGeometry* out) {
    if (SDL_strcasecmp(s, "console") == 0) {
        *out = PSYZ_GEOMETRY_CONSOLE;
    } else if (SDL_strcasecmp(s, "precise") == 0) {
        *out = PSYZ_GEOMETRY_PRECISE;
    } else if (SDL_strcasecmp(s, "perspective") == 0) {
        *out = PSYZ_GEOMETRY_PERSPECTIVE;
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
    } else if (SDL_strcasecmp(name, "frame_rate") == 0) {
        if (ParseFrameRate(value, &set->frameRate) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: frame_rate wants 60, display or %d to %d\n", path, lineNo,
                PACING_FRAME_RATE_MIN, PACING_FRAME_RATE_MAX);
    } else if (SDL_strcasecmp(name, "draw_distance") == 0) {
        if (ParseDrawDistance(value, &set->drawDistance) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: draw_distance wants %d to %d\n", path, lineNo,
                DRAW_DISTANCE_MIN, DRAW_DISTANCE_MAX);
    } else if (SDL_strcasecmp(name, "dither") == 0) {
        if (ParseOnOff(value, &set->dither) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: dither wants on or off\n", path, lineNo);
    } else if (SDL_strcasecmp(name, "colour") == 0) {
        if (ParseColour(value, &set->colour) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: colour wants console or full\n", path, lineNo);
    } else if (SDL_strcasecmp(name, "geometry") == 0) {
        if (ParseGeometry(value, &set->geometry) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: geometry wants console, precise or perspective\n", path,
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
        error("--smooth wants on or off (got %s)", args->smooth);
        return -1;
    }
    if (args->frameRate != NULL && ParseFrameRate(args->frameRate, &set.frameRate) != 0) {
        error("--frame-rate wants 60, display or %d to %d (got %s)", PACING_FRAME_RATE_MIN,
              PACING_FRAME_RATE_MAX, args->frameRate);
        return -1;
    }
    if (args->drawDistance != NULL &&
        ParseDrawDistance(args->drawDistance, &set.drawDistance) != 0) {
        error("--draw-distance wants %d to %d (got %s)", DRAW_DISTANCE_MIN, DRAW_DISTANCE_MAX,
              args->drawDistance);
        return -1;
    }
    if (args->dither != NULL && ParseOnOff(args->dither, &set.dither) != 0) {
        error("--dither wants on or off (got %s)", args->dither);
        return -1;
    }
    if (args->colour != NULL && ParseColour(args->colour, &set.colour) != 0) {
        error("--colour wants console or full (got %s)", args->colour);
        return -1;
    }
    if (args->geometry != NULL && ParseGeometry(args->geometry, &set.geometry) != 0) {
        error("--geometry wants console, precise or perspective (got %s)", args->geometry);
        return -1;
    }

    Widescreen_Init(set.aspect);
    Psyz_VideoSetInternalResolution((unsigned)set.resolution);
    Psyz_VideoSetScaleMode(set.scale);
    Pacing_Init(set.pace, set.smooth, set.frameRate);
    DrawDistance_Init(set.drawDistance);
    Psyz_VideoSetDitheringMode(set.dither ? PSYZ_DITHER_AUTO : PSYZ_DITHER_OFF);
    Psyz_VideoSetColorDepth(set.colour);
    if (Psyz_VideoSetGeometry(set.geometry) != 0) {
        fprintf(stderr, "lsd: geometry: this build has only the console's "
                        "(LSD_PRECISE_GEOMETRY is off)\n");
    }
    return 0;
}
