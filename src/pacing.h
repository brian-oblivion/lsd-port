#ifndef LSD_PACING_H
#define LSD_PACING_H

// The dream's pace, its ticks a second (src/pacing.c).
#define PACING_PACE_GAME 20 // the game's own: VSync(3)
#define PACING_PACE_MIN 10
#define PACING_PACE_MAX 30

// The console's vertical blanks a second, which the pace counts in.
#define PACING_BLANK_RATE 59.94

// frame_rate: the dream's frames a second with smooth on.
#define PACING_FRAME_RATE_CONSOLE 0  // one per blank, 59.94
#define PACING_FRAME_RATE_DISPLAY -1 // the display's refresh rate
#define PACING_FRAME_RATE_MIN 30
#define PACING_FRAME_RATE_MAX 360

// Runs the dream at `pace` ticks a second and, with `smooth`, draws frames
// between its ticks (src/pacing.c), at `frameRate` frames a second (or a
// PACING_FRAME_RATE_ value). At 20 without smooth the game paces itself,
// untouched. Call before the game starts.
void Pacing_Init(int pace, int smooth, int frameRate);

#endif
