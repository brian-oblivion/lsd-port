#ifndef LSD_CONTROLS_H
#define LSD_CONTROLS_H

// Reads <savesDir>controls.ini (writing it with the defaults when missing)
// and sets pad 1's keyboard map from it. savesDir ends in a separator.
void SetUpControls(const char* savesDir);

#endif
