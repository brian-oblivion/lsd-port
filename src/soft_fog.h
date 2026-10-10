#ifndef LSD_SOFT_FOG_H
#define LSD_SOFT_FOG_H

// Soft fog (src/soft_fog.c): the edges of what the dream's map draws kept in
// the fog, and what it starts to draw faded in from it. Off is the console's.
// Call before the game starts.
void SoftFog_Init(int on);

// The same while the game runs; in a dream, at once.
void SoftFog_Set(int on);

#endif
