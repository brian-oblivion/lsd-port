#ifndef LSD_FAR_MAP_H
#define LSD_FAR_MAP_H

// The far map (src/far_map.c): the dream's map past the seven chunks the
// game keeps loaded, loaded and drawn by the port, and the cells shown past
// the game's window for a wider or deeper view. src/far_map_cells.h has
// its cells, for the game's C.

// Holds the disc image open for the chunks' files and wraps the StageMap's,
// the dream Viewport's and DayTask's methods. Call before the game starts,
// before SoftFog_Init (which wraps the same refresh after this).
void FarMap_Init(const char* discPath);

// How far, in cells, the view reaches: the game's window (20) or, with a
// draw distance past 1, as far as the fog leaves anything (up to 30).
int FarMap_Reach(void);

#endif
