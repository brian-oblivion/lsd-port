#ifndef LSD_SETTINGS_H
#define LSD_SETTINGS_H

// The picture settings: aspect, resolution and scale, from the command line
// or environment (the arguments, NULL when not given), else from
// <savesDir>settings.ini (written with the defaults when missing), else the
// defaults. Applies them to psyz and the dream (src/widescreen.c). Returns
// 0, or -1 after StartError-style reporting when a command-line or
// environment value is malformed; a bad line in the file is reported on
// stderr and its default used. savesDir ends in a separator.
int SetUpPicture(const char* savesDir, const char* aspect, const char* resolution,
                 const char* scale, void (*error)(const char* fmt, ...));

#endif
