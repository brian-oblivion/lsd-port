#ifndef LSD_DRAW_DISTANCE_H
#define LSD_DRAW_DISTANCE_H

// draw_distance's range: the fog N times further away (src/draw_distance.c).
#define DRAW_DISTANCE_MIN 1 // the console's
#define DRAW_DISTANCE_MAX 4

// Moves the dream's fog `scale` times further away from now on, never past
// the game's clearest fog (src/draw_distance.c); 1 keeps the console's.
// Call before the game starts.
void DrawDistance_Init(int scale);

// The same while the game runs; in a dream, from its next frame.
void DrawDistance_Set(int scale);

#endif
