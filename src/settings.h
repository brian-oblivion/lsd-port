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
} SettingArgs;

// The settings: aspect, resolution, scale, pace and smooth, from the command
// line or environment (args), else from <savesDir>settings.ini (written with
// the defaults when missing), else the defaults. Applies them to psyz and the
// dream (src/widescreen.c, src/pacing.c). Returns 0, or -1 after
// StartError-style reporting when a command-line or environment value is
// malformed; a bad line in the file is reported on stderr and its default
// used. savesDir ends in a separator.
int SetUpSettings(const char* savesDir, const SettingArgs* args,
                  void (*error)(const char* fmt, ...));

#endif
