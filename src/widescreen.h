#ifndef LSD_WIDESCREEN_H
#define LSD_WIDESCREEN_H

// Parses "W:H" (for example "16:9") into W / H. Returns 0, or -1 if malformed.
int Widescreen_Parse(const char* aspect, float* ratio);

// Draws the dream at this width:height ratio from now on (src/widescreen.c);
// 4:3 or narrower keeps the console's picture. Call before the game starts.
void Widescreen_Init(float ratio);

#endif
