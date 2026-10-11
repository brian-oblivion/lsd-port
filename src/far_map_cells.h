#ifndef LSD_FAR_MAP_CELLS_H
#define LSD_FAR_MAP_CELLS_H

// The map's loaded chunks, the game's and src/far_map.c's, for code built
// with the game's C.

#include "common.h"
#include "grid_cell.h"
#include "stage_map.h"
#include "far_map.h"

// The most chunks FarMap_Chunks hands out: the game's seven and the far ones.
#define FAR_MAP_MAX_CHUNKS 48
// The ids of the far chunks start here; below are the game's slots.
#define FAR_MAP_FIRST_ID CHUNK_NEIGHBOUR_COUNT

// A loaded chunk of the map, the game's or the port's.
typedef struct {
    int id;                // the game's slot (0 to 6), or FAR_MAP_FIRST_ID on
    s32 chunk;             // its index in the stage's grid
    const GteLong* origin; // its corner in the world: x, y, z
    GridCell** cells;      // its STAGE_SLOT_LATTICE_CELLS cells, each heading a chain
} MapChunk;

// Every chunk loaded now: the game's slots, then the far chunks no slot
// holds. Returns how many it put in out (FAR_MAP_MAX_CHUNKS at most); 0 for
// a stage whose chunks are stacked in a column.
int FarMap_Chunks(StageMap* self, MapChunk* out);

// The cells of chunk id, if it still holds chunk; NULL if not.
GridCell** FarMap_Cells(StageMap* self, int id, s32 chunk);

#endif
