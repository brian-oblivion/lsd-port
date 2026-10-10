// The settings in settings.ini (in the saves folder, beside controls.ini),
// written with the defaults on first start:
//   aspect = 4:3            the dream's width:height; wider shows more of it
//   resolution = 1          the 3D drawn at N times 320x240, 1 to 8
//   scale = sharp           nearest | sharp | smooth | integer
//   pace = 14               the dream's ticks a second, 10 to 30
//   smooth = on             frames drawn between the dream's ticks
//   frame_rate = 60         their rate: 60, display, or 30 to 360
//   draw_distance = 1       the dream's fog N times further away, 1 to 4
//   fog = console           console | soft (the map's edges kept in the fog)
//   dither = on             the console's 4x4 dither pattern, or off
//   colour = console        console (15-bit) | full (24-bit, no dither)
//   geometry = console      console | precise | perspective
//   volume = 100            the whole sound, 0 to 100 %
//   music_volume = 100      the music, 0 to 100 %
//   effects_volume = 100    the sound effects, 0 to 100 %
//   movie_volume = 100      the movies' sound, 0 to 100 %
//   interpolation = console console (gaussian) | cubic | sinc
// The command line (--aspect, --resolution, --scale, --pace, --smooth,
// --frame-rate, --draw-distance, --fog, --dither, --colour, --geometry,
// --volume, --music-volume, --effects-volume, --movie-volume,
// --interpolation) and the environment (LSD_ASPECT, LSD_RESOLUTION,
// LSD_SCALE, LSD_PACE, LSD_SMOOTH, LSD_FRAME_RATE, LSD_DRAW_DISTANCE,
// LSD_FOG, LSD_DITHER, LSD_COLOUR, LSD_GEOMETRY, LSD_VOLUME,
// LSD_MUSIC_VOLUME, LSD_EFFECTS_VOLUME, LSD_MOVIE_VOLUME,
// LSD_INTERPOLATION) win over the file.
//
// The volumes and the interpolation change what is heard only (psyz's
// Psyz_SpuSetGroupGain and the like): the SPU's registers, envelopes and
// capture buffers, which the game can read, stay the console's.
//
// The settings menu (src/menu.cpp) changes them while the game runs and
// writes those it changed back into the file (Ini_Update), leaving the
// ones the command line or the environment gave alone. Aspect shows from the
// next dream (src/widescreen.c); the rest at once.

#include "settings.h"
#include "draw_distance.h"
#include "ini.h"
#include "pacing.h"
#include "soft_fog.h"
#include "widescreen.h"

#include <psyz.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char aspect[24]; // "W:H"
    int resolution;
    PsyzScaleMode scale;
    int pace;
    int smooth;
    int frameRate;
    int drawDistance;
    int fog; // 1: soft
    int dither;
    PsyzColorDepth colour;
    PsyzGeometry geometry;
    int volume[4]; // whole, music, effects, movies: 0 to 100
    PsyzSpuInterp interpolation;
} Settings;

static const Settings sDefaults = {
    "4:3", 1, PSYZ_SCALE_SHARP, 14, 1, PACING_FRAME_RATE_CONSOLE, 1, 0, 1,
    PSYZ_COLOR_DEPTH_15, PSYZ_GEOMETRY_CONSOLE, {100, 100, 100, 100}, PSYZ_SPU_INTERP_GAUSS,
};

#define STR_(x) #x
#define STR(x) STR_(x)

// Each setting's name in the file, option, environment variable and what it
// takes.
static const struct {
    const char* name;
    const char* option;
    const char* env;
    const char* wants;
} sInfo[SETTING_COUNT] = {
    {"aspect", "--aspect", "LSD_ASPECT", "width:height, such as 16:9"},
    {"resolution", "--resolution", "LSD_RESOLUTION", "1 to " STR(PSYZ_INTERNAL_RES_MAX)},
    {"scale", "--scale", "LSD_SCALE", "nearest, sharp, smooth or integer"},
    {"pace", "--pace", "LSD_PACE", STR(PACING_PACE_MIN) " to " STR(PACING_PACE_MAX)},
    {"smooth", "--smooth", "LSD_SMOOTH", "on or off"},
    {"frame_rate", "--frame-rate", "LSD_FRAME_RATE",
     "60, display or " STR(PACING_FRAME_RATE_MIN) " to " STR(PACING_FRAME_RATE_MAX)},
    {"draw_distance", "--draw-distance", "LSD_DRAW_DISTANCE",
     STR(DRAW_DISTANCE_MIN) " to " STR(DRAW_DISTANCE_MAX)},
    {"fog", "--fog", "LSD_FOG", "console or soft"},
    {"dither", "--dither", "LSD_DITHER", "on or off"},
    {"colour", "--colour", "LSD_COLOUR", "console or full"},
    {"geometry", "--geometry", "LSD_GEOMETRY", "console, precise or perspective"},
    {"volume", "--volume", "LSD_VOLUME", "0 to 100"},
    {"music_volume", "--music-volume", "LSD_MUSIC_VOLUME", "0 to 100"},
    {"effects_volume", "--effects-volume", "LSD_EFFECTS_VOLUME", "0 to 100"},
    {"movie_volume", "--movie-volume", "LSD_MOVIE_VOLUME", "0 to 100"},
    {"interpolation", "--interpolation", "LSD_INTERPOLATION", "console, cubic or sinc"},
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
    "# --scale, --pace, --smooth, --frame-rate, --draw-distance, --fog,\n"
    "# --dither, --colour, --geometry, --volume, --music-volume,\n"
    "# --effects-volume, --movie-volume, --interpolation) and the environment\n"
    "# (LSD_ASPECT, LSD_RESOLUTION, LSD_SCALE, LSD_PACE, LSD_SMOOTH,\n"
    "# LSD_FRAME_RATE, LSD_DRAW_DISTANCE, LSD_FOG, LSD_DITHER, LSD_COLOUR,\n"
    "# LSD_GEOMETRY, LSD_VOLUME, LSD_MUSIC_VOLUME, LSD_EFFECTS_VOLUME,\n"
    "# LSD_MOVIE_VOLUME, LSD_INTERPOLATION) win over this file.\n"
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
    "# console: the PS1's fog, so distant things pop into view at the edges of\n"
    "# what the dream draws. soft: those edges kept in the fog, and what comes\n"
    "# into view there faded in from it.\n"
    "fog = console\n"
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
    "geometry = console\n"
    "\n"
    "# Volumes, 0 to 100 (%): all the sound, then the music, the sound\n"
    "# effects and the movies' sound within it. 100 is the console's.\n"
    "volume = 100\n"
    "music_volume = 100\n"
    "effects_volume = 100\n"
    "movie_volume = 100\n"
    "\n"
    "# How the sound's samples are smoothed as they play at each note's pitch:\n"
    "#   console  the PS1's gaussian, soft and a little muffled\n"
    "#   cubic    brighter, close to the original samples\n"
    "#   sinc     the brightest and cleanest\n"
    "interpolation = console\n";

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

static const char* const sGeometries[] = {"console", "precise", "perspective"};

// Whether this build draws more than the console's geometry.
#ifdef LSD_PRECISE_GEOMETRY
static const int sGeometryBuilt = 1;
#else
static const int sGeometryBuilt = 0;
#endif

static int ParseAspect(const char* s, char* out, size_t size) {
    float ratio;
    unsigned w, h;
    if (Widescreen_Parse(s, &ratio) != 0 || sscanf(s, "%u:%u", &w, &h) != 2) {
        return -1;
    }
    SDL_snprintf(out, size, "%u:%u", w, h);
    return 0;
}

static int ParseFog(const char* s, int* out) {
    if (SDL_strcasecmp(s, "console") == 0) {
        *out = 0;
    } else if (SDL_strcasecmp(s, "soft") == 0) {
        *out = 1;
    } else {
        return -1;
    }
    return 0;
}

static int ParseVolume(const char* s, int* out) {
    char* end;
    long n = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || n < 0 || n > 100) {
        return -1;
    }
    *out = (int)n;
    return 0;
}

static const char* const sInterpolations[] = {"console", "cubic", "sinc"};

static int ParseInterpolation(const char* s, PsyzSpuInterp* out) {
    for (int i = 0; i < (int)SDL_arraysize(sInterpolations); i++) {
        if (SDL_strcasecmp(s, sInterpolations[i]) == 0) {
            *out = (PsyzSpuInterp)i;
            return 0;
        }
    }
    return -1;
}

// Parses one setting into set. Returns 0, or -1 if malformed.
static int Parse(SettingId id, const char* s, Settings* set) {
    switch (id) {
    case SETTING_ASPECT:
        return ParseAspect(s, set->aspect, sizeof(set->aspect));
    case SETTING_RESOLUTION:
        return ParseResolution(s, &set->resolution);
    case SETTING_SCALE:
        return ParseScale(s, &set->scale);
    case SETTING_PACE:
        return ParsePace(s, &set->pace);
    case SETTING_SMOOTH:
        return ParseOnOff(s, &set->smooth);
    case SETTING_FRAME_RATE:
        return ParseFrameRate(s, &set->frameRate);
    case SETTING_DRAW_DISTANCE:
        return ParseDrawDistance(s, &set->drawDistance);
    case SETTING_FOG:
        return ParseFog(s, &set->fog);
    case SETTING_DITHER:
        return ParseOnOff(s, &set->dither);
    case SETTING_COLOUR:
        return ParseColour(s, &set->colour);
    case SETTING_GEOMETRY:
        return ParseGeometry(s, &set->geometry);
    case SETTING_VOLUME:
    case SETTING_MUSIC_VOLUME:
    case SETTING_EFFECTS_VOLUME:
    case SETTING_MOVIE_VOLUME:
        return ParseVolume(s, &set->volume[id - SETTING_VOLUME]);
    case SETTING_INTERPOLATION:
        return ParseInterpolation(s, &set->interpolation);
    default:
        return -1;
    }
}

// One setting of set as settings.ini writes it, into out.
static void Format(SettingId id, const Settings* set, char* out, size_t size) {
    switch (id) {
    case SETTING_ASPECT:
        SDL_strlcpy(out, set->aspect, size);
        break;
    case SETTING_RESOLUTION:
        SDL_snprintf(out, size, "%d", set->resolution);
        break;
    case SETTING_SCALE:
        for (int i = 0; i < SCALE_COUNT; i++) {
            if (sScales[i].mode == set->scale) {
                SDL_strlcpy(out, sScales[i].name, size);
            }
        }
        break;
    case SETTING_PACE:
        SDL_snprintf(out, size, "%d", set->pace);
        break;
    case SETTING_SMOOTH:
        SDL_strlcpy(out, set->smooth ? "on" : "off", size);
        break;
    case SETTING_FRAME_RATE:
        if (set->frameRate == PACING_FRAME_RATE_CONSOLE) {
            SDL_strlcpy(out, "60", size);
        } else if (set->frameRate == PACING_FRAME_RATE_DISPLAY) {
            SDL_strlcpy(out, "display", size);
        } else {
            SDL_snprintf(out, size, "%d", set->frameRate);
        }
        break;
    case SETTING_DRAW_DISTANCE:
        SDL_snprintf(out, size, "%d", set->drawDistance);
        break;
    case SETTING_FOG:
        SDL_strlcpy(out, set->fog ? "soft" : "console", size);
        break;
    case SETTING_DITHER:
        SDL_strlcpy(out, set->dither ? "on" : "off", size);
        break;
    case SETTING_COLOUR:
        SDL_strlcpy(out, set->colour == PSYZ_COLOR_DEPTH_24 ? "full" : "console", size);
        break;
    case SETTING_GEOMETRY:
        SDL_strlcpy(out, sGeometries[set->geometry], size);
        break;
    case SETTING_VOLUME:
    case SETTING_MUSIC_VOLUME:
    case SETTING_EFFECTS_VOLUME:
    case SETTING_MOVIE_VOLUME:
        SDL_snprintf(out, size, "%d", set->volume[id - SETTING_VOLUME]);
        break;
    case SETTING_INTERPOLATION:
        SDL_strlcpy(out, sInterpolations[set->interpolation], size);
        break;
    default:
        out[0] = '\0';
        break;
    }
}

// IniApplyFn for settings.ini, into the Settings ctx.
static int ApplySetting(void* ctx, const char* name, char* value, const char* path, int lineNo) {
    for (int id = 0; id < SETTING_COUNT; id++) {
        if (SDL_strcasecmp(name, sInfo[id].name) != 0) {
            continue;
        }
        if (Parse(id, value, ctx) == 0) {
            return 0;
        }
        fprintf(stderr, "lsd: %s:%d: %s wants %s\n", path, lineNo, name, sInfo[id].wants);
        return -1;
    }
    fprintf(stderr, "lsd: %s:%d: no setting %s\n", path, lineNo, name);
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

// The settings in use, and as settings.ini has them (the defaults where it
// doesn't): the menu saves those that differ.
static Settings sSet;
static Settings sFile;
static const char* sFrom[SETTING_COUNT];
static char sText[SETTING_COUNT][24]; // sSet's, formatted
static unsigned sPending;             // settings changed but not yet applied
static char* sPath;                   // settings.ini

// A volume's gain: the square of its fraction, so that 50 sounds about half
// as loud (-12 dB) rather than hardly quieter (-6 dB).
static float Gain(int volume) {
    float f = volume / 100.0f;
    return f * f;
}

static void Apply(SettingId id) {
    float ratio;
    switch (id) {
    case SETTING_ASPECT:
        Widescreen_Parse(sSet.aspect, &ratio);
        Widescreen_Set(ratio);
        break;
    case SETTING_RESOLUTION:
        Psyz_VideoSetInternalResolution((unsigned)sSet.resolution);
        break;
    case SETTING_SCALE:
        Psyz_VideoSetScaleMode(sSet.scale);
        break;
    case SETTING_PACE:
    case SETTING_SMOOTH:
    case SETTING_FRAME_RATE:
        Pacing_Set(sSet.pace, sSet.smooth, sSet.frameRate);
        break;
    case SETTING_DRAW_DISTANCE:
        DrawDistance_Set(sSet.drawDistance);
        break;
    case SETTING_FOG:
        SoftFog_Set(sSet.fog);
        break;
    case SETTING_DITHER:
        Psyz_VideoSetDitheringMode(sSet.dither ? PSYZ_DITHER_AUTO : PSYZ_DITHER_OFF);
        break;
    case SETTING_COLOUR:
        Psyz_VideoSetColorDepth(sSet.colour);
        break;
    case SETTING_GEOMETRY:
        if (Psyz_VideoSetGeometry(sSet.geometry) != 0) {
            fprintf(stderr, "lsd: geometry: this build has only the console's "
                            "(LSD_PRECISE_GEOMETRY is off)\n");
        }
        break;
    case SETTING_VOLUME:
        Psyz_SpuSetMasterGain(Gain(sSet.volume[0]));
        break;
    case SETTING_MUSIC_VOLUME:
        Psyz_SpuSetGroupGain(PSYZ_SPU_GROUP_SEQ, Gain(sSet.volume[1]));
        break;
    case SETTING_EFFECTS_VOLUME:
        Psyz_SpuSetGroupGain(PSYZ_SPU_GROUP_SE, Gain(sSet.volume[2]));
        break;
    case SETTING_MOVIE_VOLUME:
        Psyz_SpuSetCdGain(Gain(sSet.volume[3]));
        break;
    case SETTING_INTERPOLATION:
        Psyz_SpuSetInterpolation(sSet.interpolation);
        break;
    default:
        break;
    }
}

void Settings_FromEnv(SettingArgs* args) {
    for (int id = 0; id < SETTING_COUNT; id++) {
        const char* value = getenv(sInfo[id].env);
        if (value != NULL) {
            args->value[id] = value;
            args->from[id] = sInfo[id].env;
        }
    }
}

int Settings_FromArg(SettingArgs* args, const char* option, const char* value) {
    for (int id = 0; id < SETTING_COUNT; id++) {
        if (SDL_strcmp(option, sInfo[id].option) == 0) {
            args->value[id] = value;
            args->from[id] = sInfo[id].option;
            return 1;
        }
    }
    return 0;
}

int SetUpSettings(const char* savesDir, const SettingArgs* args,
                  void (*error)(const char* fmt, ...)) {
    sSet = sDefaults;
    SDL_asprintf(&sPath, "%ssettings.ini", savesDir);
    if (Ini_Read(sPath, ApplySetting, &sSet) != 0) {
        WriteDefaults(sPath);
    }
    sFile = sSet;

    for (int id = 0; id < SETTING_COUNT; id++) {
        if (args->value[id] == NULL) {
            continue;
        }
        if (Parse(id, args->value[id], &sSet) != 0) {
            error("%s wants %s (got %s)", args->from[id], sInfo[id].wants, args->value[id]);
            return -1;
        }
        sFrom[id] = args->from[id];
    }
    for (int id = 0; id < SETTING_COUNT; id++) {
        Format(id, &sSet, sText[id], sizeof(sText[id]));
    }

    float ratio;
    Widescreen_Parse(sSet.aspect, &ratio);
    Widescreen_Init(ratio);
    Pacing_Init(sSet.pace, sSet.smooth, sSet.frameRate);
    DrawDistance_Init(sSet.drawDistance);
    SoftFog_Init(sSet.fog);
    for (int id = SETTING_RESOLUTION; id < SETTING_COUNT; id++) {
        if (id != SETTING_PACE && id != SETTING_SMOOTH && id != SETTING_FRAME_RATE &&
            id != SETTING_DRAW_DISTANCE && id != SETTING_FOG) {
            Apply(id);
        }
    }
    return 0;
}

const char* Settings_Name(SettingId id) {
    return sInfo[id].name;
}

const char* Settings_Value(SettingId id) {
    return sText[id];
}

const char* Settings_From(SettingId id) {
    return sFrom[id];
}

int Settings_NextDream(SettingId id) {
    return id == SETTING_ASPECT;
}

int Settings_Accepts(SettingId id, const char* value) {
    Settings set = sSet;
    if (sFrom[id] != NULL || Parse(id, value, &set) != 0) {
        return 0;
    }
    return id != SETTING_GEOMETRY || set.geometry == PSYZ_GEOMETRY_CONSOLE || sGeometryBuilt;
}

int Settings_Set(SettingId id, const char* value) {
    if (!Settings_Accepts(id, value)) {
        return -1;
    }
    Parse(id, value, &sSet);
    Format(id, &sSet, sText[id], sizeof(sText[id]));
    sPending |= 1u << id;
    return 0;
}

void Settings_Apply(void) {
    unsigned pending = sPending;
    sPending = 0;
    for (int id = 0; id < SETTING_COUNT; id++) {
        if (pending & (1u << id)) {
            Apply(id);
        }
    }
}

int Settings_Save(void) {
    IniSetting changed[SETTING_COUNT];
    char file[SETTING_COUNT][24];
    int count = 0;
    for (int id = 0; id < SETTING_COUNT; id++) {
        Format(id, &sFile, file[id], sizeof(file[id]));
        if (sFrom[id] == NULL && SDL_strcmp(file[id], sText[id]) != 0) {
            changed[count].name = sInfo[id].name;
            changed[count].value = sText[id];
            count++;
        }
    }
    if (count == 0) {
        return 0;
    }
    if (Ini_Update(sPath, changed, count) != 0) {
        return -1;
    }
    for (int id = 0; id < SETTING_COUNT; id++) {
        if (sFrom[id] == NULL) {
            Parse(id, sText[id], &sFile);
        }
    }
    return 0;
}
