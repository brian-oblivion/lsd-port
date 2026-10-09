#ifndef LSD_SETTINGS_H
#define LSD_SETTINGS_H

// The settings given on the command line or in the environment; NULL when
// not given.
typedef struct {
    const char* aspect;
    const char* resolution;
    const char* scale;
    const char* pace;
    const char* smooth;
    const char* frameRate;
    const char* drawDistance;
    const char* dither;
    const char* colour;
    const char* geometry;
} SettingArgs;

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

#endif
