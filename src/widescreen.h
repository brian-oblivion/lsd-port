#ifndef LSD_WIDESCREEN_H
#define LSD_WIDESCREEN_H

// Parses "W:H" (for example "16:9") into W / H. Returns 0, or -1 if malformed.
int Widescreen_Parse(const char* aspect, float* ratio);

// Draws the dream at this width:height ratio from now on (src/widescreen.c);
// 4:3 or narrower keeps the console's picture. Call before the game starts:
// it also shapes the window.
void Widescreen_Init(float ratio);

// The ratio from the next dream on, while the game runs.
void Widescreen_Set(float ratio);

// How much wider than 4:3 the dream being drawn is: 1 at 4:3 and outside a
// dream.
float Widescreen_Stretch(void);

#endif
