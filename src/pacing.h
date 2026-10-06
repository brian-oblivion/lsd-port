#ifndef LSD_PACING_H
#define LSD_PACING_H

// The dream's pace, its ticks a second (src/pacing.c).
#define PACING_PACE_GAME 20 // the game's own: VSync(3)
#define PACING_PACE_MIN 10
#define PACING_PACE_MAX 30

// Runs the dream at `pace` ticks a second and, with `smooth`, draws frames
// between its ticks (src/pacing.c). At 20 without smooth the game paces
// itself, untouched. Call before the game starts.
void Pacing_Init(int pace, int smooth);

#endif
