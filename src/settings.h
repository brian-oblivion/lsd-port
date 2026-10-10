#ifndef LSD_SETTINGS_H
#define LSD_SETTINGS_H

// The settings, in the order settings.ini and the menu list them.
typedef enum {
    SETTING_ASPECT,
    SETTING_RESOLUTION,
    SETTING_SCALE,
    SETTING_PACE,
    SETTING_SMOOTH,
    SETTING_FRAME_RATE,
    SETTING_DRAW_DISTANCE,
    SETTING_DITHER,
    SETTING_COLOUR,
    SETTING_GEOMETRY,
    SETTING_COUNT
} SettingId;

// The settings given on the command line or in the environment: the value,
// or NULL when not given, and where it came from ("--aspect", "LSD_ASPECT").
typedef struct {
    const char* value[SETTING_COUNT];
    const char* from[SETTING_COUNT];
} SettingArgs;

// Fills args from the environment (LSD_ASPECT, ...).
void Settings_FromEnv(SettingArgs* args);

// When option is a setting's (--aspect, ...), takes value for it and returns
// 1; otherwise 0.
int Settings_FromArg(SettingArgs* args, const char* option, const char* value);

// The settings: aspect, resolution, scale, pace, smooth, frame_rate,
// draw_distance, dither, colour and geometry, from the command line or environment
// (args), else from <savesDir>settings.ini (written with the defaults when
// missing), else the defaults. Applies them to psyz and the dream
// (src/widescreen.c, src/pacing.c, src/draw_distance.c). Returns 0, or -1
// after StartError-style reporting when a command-line or environment value
// is malformed; a bad line in the file is reported on stderr and its default
// used. savesDir ends in a separator.
int SetUpSettings(const char* savesDir, const SettingArgs* args,
                  void (*error)(const char* fmt, ...));

// For the settings menu (src/menu.cpp), once SetUpSettings has run.

// The setting's name in settings.ini ("frame_rate").
const char* Settings_Name(SettingId id);

// Its value as settings.ini writes it ("16:9", "display", "on").
const char* Settings_Value(SettingId id);

// Where the value came from for this run when the command line or the
// environment gave it ("--pace", "LSD_PACE"): the file's is not used, and
// Settings_Set refuses to change it. NULL otherwise.
const char* Settings_From(SettingId id);

// Whether a change shows only from the next dream on (aspect), rather than
// at once.
int Settings_NextDream(SettingId id);

// Whether Settings_Set would take this value: well formed, the setting not
// given for this run, and (geometry) built in.
int Settings_Accepts(SettingId id, const char* value);

// Changes a setting, as settings.ini would write it. The value reads back at
// once; it reaches the game at the next Settings_Apply. Returns 0, or -1
// when the value is malformed or the setting was given for this run.
int Settings_Set(SettingId id, const char* value);

// Hands the settings changed since the last call to psyz and the dream.
// Call between the game's frames (from a VSync callback, say), not while
// psyz presents.
void Settings_Apply(void);

// Writes the settings changed since they were read (or last saved) to
// settings.ini, keeping its comments and other lines. Returns 0, or -1 when
// it cannot be written.
int Settings_Save(void);

#endif
