// Soft fog: the edges of what the map draws hidden in the fog, and what it
// starts to draw faded in from it, so that nothing pops into view.
//
// The StageMap draws only the cells in a window ahead of the player (20 x 20
// cells, axis-aligned, shifted toward the look direction; src/widescreen.c
// widens it at 16:9) and hides the rest with GsDOFF. As the player turns or
// walks, whole rows of cells are switched on at the window's edges, and the
// game's fog, which depends only on depth, is often thin there: on its
// clearest stages a cell 20 cells ahead is less than half fogged, and one at
// the side of a 16:9 view not at all. So distant things pop in and out.
//
// Two things, through psyz's depth-cue hook, which gives a vertex the fog
// the game already has (the GTE's depth cue, IR0, which the TMD renderer
// turns into the stage's fog palette rows and fog colour, and culls at ONE)
// and a fade (psyz draws the polygon that much transparent):
//  - edge fog: a cell the map shows, near a cell in view that it hides and
//    that has something in it, is fogged by how near: fully next to it, not
//    at all EDGE_BAND cells away. So the window's edges are in the fog where
//    they can be seen, and a cell that comes into view there was fogged
//    already. Hidden cells out of view fog nothing.
//  - fade-in: a cell that starts to be shown starts at full fog and clears
//    to its edge fog over FADE_SECONDS of frame time.
// A cell's fog (0..1) is the fog colour over its first FOG_SHARE, never to
// ONE, and the fade over its last FADE_SHARE: so a cell at full fog is not
// drawn at all, and one coming into view dissolves in, in the fog colour,
// before the fog clears. Without psyz's fade (a build without
// PSYZ_PRECISE_GEOMETRY) the fog goes to ONE instead, where the game culls.
// Both are worked out per cell at each tick, when the StageMap has refreshed
// its window (refreshFootprint, wrapped after src/widescreen.c's so its
// extra cells count), and eased from frame to frame (the Viewport's update,
// wrapped). The hook takes a vertex's view position back to the world with
// GsWSMATRIX and blends the fog of the four nearest cell centres, laid over
// the game's own: dp + (ONE - dp) * fog. None of it is near the player
// (NEAR_CELLS), where nothing pops.
//
// Stages whose chunks are stacked in a column (the grid's isVertical) show
// whole chunks, the player's and the next by where the player stands
// (StageMap__SetFootprintFromQuery), not a window ahead; they are left as
// they are.
//
// Drawing only: the game reads IR0 only to fog and cull faces, and the fade
// only reaches the GPU. Off (the default), no hook is set, and the GTE gives
// the game's own depth cue.
//
// Built with the game's C (it needs StageMap's, DayTask's and the Viewport's
// method tables), not with the port's other files.

#include "common.h"
#include "day_task.h"
#include "grid_cell.h"
#include "lbd_file.h"
#include "node_guarded_viewport.h"
#include "stage_grid.h"
#include "stage_map.h"
#include "soft_fog.h"

#include <libgs.h>
#include <psyz/gte.h>
#include <SDL3/SDL_timer.h>
#include <string.h>

// How far, in cells, the edge fog reaches into the shown cells.
#define EDGE_BAND 4.0f
// Within this many cells of the player there is no edge fog; it comes in
// fully by NEAR_CELLS + NEAR_RAMP.
#define NEAR_CELLS 3.0f
#define NEAR_RAMP 3.0f
// How long a cell takes to clear from fully fogged, and how long a cell
// takes to fog over when an edge comes near it.
#define FADE_SECONDS 0.8f
// With psyz's fade: of a cell's fog (0..1), the share over which it fogs
// over, and the last share, over which it turns transparent.
#define FOG_SHARE 0.7f
#define FADE_SHARE 0.6f
// The view drawn in a circle (ShapeView): its radius in cells (the game's
// window reaches 20 ahead), the cells shown all round, and how far past the
// view's sides the cells are drawn already for a turn.
#define VIEW_RADIUS 21.0f
#define NEAR_SHOW 3.0f
#define TURN_MARGIN 0.45f // radians, about 26 degrees: four ticks of turning

// The cells' fog, by world cell, in a square that wraps around: the loaded
// chunks span at most 60 cells, so no two of them share a slot.
#define GRID_BITS 7
#define GRID (1 << GRID_BITS)
#define GRID_MASK (GRID - 1)

typedef struct {
    s32 cx, cz;     // the world cell this slot holds
    u32 tick;       // the refresh that last saw it loaded; 0: never
    float fog;      // shown now, 0..1
    float target;   // what it eases to
    u8 shown;       // drawn at the last refresh
    u8 hasModel;    // has something to draw
} FogCell;

static int sEnabled;
static int sNextEnabled;
static int sInDream; // a DayTask is initialised
static int sActive;  // in a dream with it on: tracking the cells, the hook set
static int sFade;    // psyz draws the hook's fade
static FogCell sCells[GRID * GRID];
static u32 sTick; // refreshes so far; FogCell.tick is one of them
static float sSlope = 160.0f / 266.0f, sSlopeMargin = 1.0f; // the view cone (InView)
static u64 sLastNs;

static void (*sRefreshFootprint)(StageMap* self);
static void (*sViewportUpdate)(NodeGuardedViewport* self);
static void (*sDayTaskOnInit)(DayTask* self, s32 a, s32 b, s32 c);
static void (*sDayTaskOnDeinit)(DayTask* self);

static FogCell* CellAt(s32 cx, s32 cz) {
    return &sCells[(cx & GRID_MASK) + (cz & GRID_MASK) * GRID];
}

static float Smooth(float t) {
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return t * t * (3.0f - 2.0f * t);
}

// The cell's fog at the vertex: 0 for a cell no refresh has seen loaded
// lately (nothing is drawn there).
static float CellFog(s32 cx, s32 cz) {
    FogCell* c = CellAt(cx, cz);
    if (c->tick != sTick || c->cx != cx || c->cz != cz) {
        return 0.0f;
    }
    return c->fog;
}

static int DepthCue(int x, int y, int z, int dp, int* fade) {
    const MATRIX* ws = &GsWSMATRIX;
    float vx = (float)(x - ws->t[0]), vy = (float)(y - ws->t[1]), vz = (float)(z - ws->t[2]);
    // The world position: GsWSMATRIX's rotation, transposed (it is
    // orthonormal, in 4096ths).
    float wx = (ws->m[0][0] * vx + ws->m[1][0] * vy + ws->m[2][0] * vz) / 4096.0f;
    float wz = (ws->m[0][2] * vx + ws->m[1][2] * vy + ws->m[2][2] * vz) / 4096.0f;
    // Between the four nearest cell centres.
    float gx = wx / STAGE_CELL_SIZE - 0.5f, gz = wz / STAGE_CELL_SIZE - 0.5f;
    float fx0 = SDL_floorf(gx), fz0 = SDL_floorf(gz);
    float tx = gx - fx0, tz = gz - fz0;
    s32 cx = (s32)fx0, cz = (s32)fz0;
    float fog = (CellFog(cx, cz) * (1.0f - tx) + CellFog(cx + 1, cz) * tx) * (1.0f - tz) +
                (CellFog(cx, cz + 1) * (1.0f - tx) + CellFog(cx + 1, cz + 1) * tx) * tz;
    if (fog <= 0.0f) {
        return dp;
    }
    if (dp < 0) {
        dp = 0;
    }
    if (!sFade) {
        return dp + (int)((ONE - dp) * fog + 0.5f);
    }
    // Fogged over the first FOG_SHARE, never to ONE (where the game culls a
    // face), and drawn more and more transparent over the last FADE_SHARE.
    *fade = (int)(ONE * Smooth((fog - (1.0f - FADE_SHARE)) / FADE_SHARE) + 0.5f);
    dp += (int)((ONE - dp) * Smooth(fog / FOG_SHARE) + 0.5f);
    return dp < ONE ? dp : ONE - 1;
}

static int CellHasModel(GridCell* cell) {
    return cell->model != NULL || cell->nextInCell != NULL;
}

static int CellShown(GridCell* cell) {
    for (; cell != NULL; cell = cell->nextInCell) {
        if (!(cell->attribute & GsDOFF)) {
            return 1;
        }
    }
    return 0;
}

// How much of the fog a cell gets for its distance from the player: none
// near, all from NEAR_CELLS + NEAR_RAMP cells out.
static float NearWeight(StageMap* self, s32 cx, s32 cz) {
    GteLong* pos = self->target->coord2->coord.t;
    float dx = (cx + 0.5f) - pos[0] / (float)STAGE_CELL_SIZE;
    float dz = (cz + 0.5f) - pos[2] / (float)STAGE_CELL_SIZE;
    return Smooth((SDL_sqrtf(dx * dx + dz * dz) - NEAR_CELLS) / NEAR_RAMP);
}

// Whether a cell is in the view cone at the player's yaw: its centre ahead,
// and inside the cone give or take VIEW_MARGIN to the sides. An edge out of
// view fogs nothing; one that comes into view as the player turns fogs what
// is near it from then on.
#define VIEW_MARGIN (1.5f * STAGE_CELL_SIZE)
static int InView(StageMap* self, s32 cx, s32 cz) {
    GsCOORD2PARAM* param = self->target->coord2->param;
    GteLong* pos = self->target->coord2->coord.t;
    float yaw = param->rotate.vy * (6.2831853f / ONE);
    float fx = SDL_sinf(yaw), fz = SDL_cosf(yaw);
    float dx = (cx + 0.5f) * STAGE_CELL_SIZE - pos[0];
    float dz = (cz + 0.5f) * STAGE_CELL_SIZE - pos[2];
    float along = dx * fx + dz * fz, across = SDL_fabsf(dx * fz - dz * fx);
    return along > 0.0f && across <= along * sSlope + VIEW_MARGIN * sSlopeMargin;
}

// Distance (in cells) from each cell of a square around the player to the
// nearest hidden cell with something in it: a two-pass chamfer transform.
#define LOCAL 96
static float sDist[LOCAL * LOCAL];

static void ComputeEdges(StageMap* self) {
    GteLong* pos = self->target->coord2->coord.t;
    s32 px = pos[0] >> STAGE_CELL_SHIFT, pz = pos[2] >> STAGE_CELL_SHIFT;
    s32 ox = px - LOCAL / 2, oz = pz - LOCAL / 2;
    const float far = 1e6f, d1 = 1.0f, d2 = 1.41421356f;
    int i, j;

    for (i = 0; i < LOCAL * LOCAL; i++) {
        sDist[i] = far;
    }
    for (j = 0; j < LOCAL; j++) {
        for (i = 0; i < LOCAL; i++) {
            FogCell* c = CellAt(ox + i, oz + j);
            int loaded = c->tick == sTick && c->cx == ox + i && c->cz == oz + j;
            float dx = (float)(ox + i - px), dz = (float)(oz + j - pz);
            // in view: a cell hidden with something in it, or (inside the
            // circle) a cell of no loaded chunk, where the map ends
            if (((loaded && c->hasModel && !c->shown) ||
                 (!loaded && dx * dx + dz * dz <= VIEW_RADIUS * VIEW_RADIUS)) &&
                InView(self, ox + i, oz + j)) {
                sDist[i + j * LOCAL] = 0.0f;
            }
        }
    }
#define D(i, j) sDist[(i) + (j) * LOCAL]
#define RELAX(i, j, ii, jj, w)                                                                     \
    if ((ii) >= 0 && (ii) < LOCAL && (jj) >= 0 && (jj) < LOCAL && D(ii, jj) + (w) < D(i, j))        \
    D(i, j) = D(ii, jj) + (w)
    for (j = 0; j < LOCAL; j++) {
        for (i = 0; i < LOCAL; i++) {
            RELAX(i, j, i - 1, j, d1);
            RELAX(i, j, i, j - 1, d1);
            RELAX(i, j, i - 1, j - 1, d2);
            RELAX(i, j, i + 1, j - 1, d2);
        }
    }
    for (j = LOCAL - 1; j >= 0; j--) {
        for (i = LOCAL - 1; i >= 0; i--) {
            RELAX(i, j, i + 1, j, d1);
            RELAX(i, j, i, j + 1, d1);
            RELAX(i, j, i + 1, j + 1, d2);
            RELAX(i, j, i - 1, j + 1, d2);
        }
    }
#undef RELAX
#undef D

    for (j = 0; j < LOCAL; j++) {
        for (i = 0; i < LOCAL; i++) {
            FogCell* c = CellAt(ox + i, oz + j);
            float edge;
            if (c->tick != sTick || c->cx != ox + i || c->cz != oz + j) {
                continue;
            }
            if (sDist[i + j * LOCAL] == 0.0f) {
                c->target = c->fog = 1.0f; // an edge: the fog its neighbours blend to
                continue;
            }
            edge = 1.0f - Smooth((sDist[i + j * LOCAL] - 1.0f) / EDGE_BAND);
            c->target = NearWeight(self, ox + i, oz + j) * edge;
        }
    }
}

// The view drawn in a circle: every loaded cell with something in it within
// VIEW_RADIUS cells, ahead in the view cone widened by TURN_MARGIN, or
// within NEAR_SHOW cells all round, is shown, and every cell further than
// VIEW_RADIUS is hidden, whatever the game's window says. So a turn brings
// cells into view that were drawn already (the window jumps sideways a cell
// at a time, and to the other axis at 45 degrees), and the edge, and its fog,
// is where it was. Drawing only, as widescreen.c's extra cells: the cells
// shown here are hidden again before the next refresh, and the game hides
// its window's itself. (VIEW_RADIUS, NEAR_SHOW and TURN_MARGIN are above.)

typedef struct {
    s16 slot;
    s16 chunk;
    s16 cell;
} ShownCell;

static StageMap* sShownMap;
static int sShownCount;
static ShownCell sShown[CHUNK_NEIGHBOUR_COUNT * STAGE_SLOT_LATTICE_CELLS];

static void SetCellShown(GridCell* cell, int shown) {
    for (; cell != NULL; cell = cell->nextInCell) {
        if (shown) {
            cell->attribute &= ~GsDOFF;
        } else {
            cell->attribute |= GsDOFF;
        }
    }
}

static void HideShown(StageMap* self) {
    int i;
    if (self == sShownMap) {
        for (i = 0; i < sShownCount; i++) {
            ChunkSlot* slot = &self->slots[sShown[i].slot];
            if (slot->loader->headerReady != 0 && slot->loader->chunkIndex == sShown[i].chunk) {
                SetCellShown(slot->cells[sShown[i].cell], 0);
            }
        }
    }
    sShownMap = self;
    sShownCount = 0;
}

static void ShapeView(StageMap* self) {
    GsCOORD2PARAM* param = self->target->coord2->param;
    GteLong* pos = self->target->coord2->coord.t;
    float yaw = param->rotate.vy * (6.2831853f / ONE);
    float fx = SDL_sinf(yaw), fz = SDL_cosf(yaw);
    float half = SDL_atanf(sSlope) + TURN_MARGIN;
    int i, k;

    for (i = 0; i < CHUNK_NEIGHBOUR_COUNT; i++) {
        ChunkSlot* slot = &self->slots[i];
        GteLong* o;
        if (slot->loader->headerReady == 0 || slot->loader->chunkIndex < 0) {
            continue;
        }
        o = slot->cellParent->coord2->coord.t;
        for (k = 0; k < STAGE_SLOT_LATTICE_CELLS; k++) {
            GridCell* cell = slot->cells[k];
            float dx = (o[0] + (k % STAGE_CHUNK_CELLS + 0.5f) * STAGE_CELL_SIZE - pos[0]) /
                       STAGE_CELL_SIZE;
            float dz = (o[2] + (k / STAGE_CHUNK_CELLS + 0.5f) * STAGE_CELL_SIZE - pos[2]) /
                       STAGE_CELL_SIZE;
            float dist = SDL_sqrtf(dx * dx + dz * dz);
            int want;
            if (!CellHasModel(cell)) {
                continue;
            }
            if (dist > VIEW_RADIUS) {
                SetCellShown(cell, 0);
                continue;
            }
            want = dist <= NEAR_SHOW;
            if (!want) {
                float along = dx * fx + dz * fz, across = SDL_fabsf(dx * fz - dz * fx);
                // the cell's half diagonal, as an angle seen from here
                want = SDL_atan2f(across, along) <= half + SDL_asinf(0.71f / dist);
            }
            if (want && !CellShown(cell)) {
                SetCellShown(cell, 1);
                sShown[sShownCount].slot = (s16)i;
                sShown[sShownCount].chunk = (s16)slot->loader->chunkIndex;
                sShown[sShownCount].cell = (s16)k;
                sShownCount++;
            }
        }
    }
}

static void SoftRefreshFootprint(StageMap* self) {
    int i, k;
    if (sActive) {
        HideShown(self);
    }
    sRefreshFootprint(self);
    if (!sActive || self->chunksLoaded == 0) {
        return;
    }
    sTick++;
    if (sTick == 0) {
        sTick = 1;
    }
    if (self->config->isVertical != 0) {
        return; // whole chunks by where the player stands: left as they are
    }
    {
        // The view's half-width over its depth: 160 / h, wider by the
        // widescreen squeeze (Psyz_GteSetScreenXScale).
        int h = (int)Psyz_GteCtrlRead(26), scale = Psyz_GteGetScreenXScale();
        sSlope = 160.0f / (float)(h > 0 ? h : 266) * 65536.0f / (float)(scale > 0 ? scale : 65536);
        sSlopeMargin = SDL_sqrtf(1.0f + sSlope * sSlope);
    }
    ShapeView(self);
    for (i = 0; i < CHUNK_NEIGHBOUR_COUNT; i++) {
        ChunkSlot* slot = &self->slots[i];
        GteLong* o;
        if (slot->loader->headerReady == 0 || slot->loader->chunkIndex < 0) {
            continue;
        }
        o = slot->cellParent->coord2->coord.t;
        for (k = 0; k < STAGE_SLOT_LATTICE_CELLS; k++) {
            GridCell* cell = slot->cells[k];
            s32 cx = (o[0] + (k % STAGE_CHUNK_CELLS) * STAGE_CELL_SIZE) >> STAGE_CELL_SHIFT;
            s32 cz = (o[2] + (k / STAGE_CHUNK_CELLS) * STAGE_CELL_SIZE) >> STAGE_CELL_SHIFT;
            FogCell* c = CellAt(cx, cz);
            int known = c->tick == sTick - 1 && c->cx == cx && c->cz == cz;
            int shown = CellShown(cell);
            if (!known) {
                c->cx = cx;
                c->cz = cz;
                c->fog = 1.0f;
                c->shown = 0;
            }
            c->hasModel = (u8)CellHasModel(cell);
            if (c->hasModel && shown && !c->shown) {
                c->fog = NearWeight(self, cx, cz); // fades in
            }
            c->shown = (u8)shown;
            c->tick = sTick;
        }
    }
    ComputeEdges(self);
}

// Each frame, every cell's fog a step nearer its target.
static void Ease(void) {
    u64 now = SDL_GetTicksNS();
    float step = sLastNs ? (float)(now - sLastNs) / 1e9f / FADE_SECONDS : 0.0f;
    int i;
    sLastNs = now;
    if (step <= 0.0f) {
        return;
    }
    if (step > 1.0f) {
        step = 1.0f;
    }
    for (i = 0; i < GRID * GRID; i++) {
        FogCell* c = &sCells[i];
        if (c->tick != sTick) {
            continue;
        }
        if (c->fog > c->target) {
            c->fog = c->fog - step > c->target ? c->fog - step : c->target;
        } else if (c->fog < c->target) {
            c->fog = c->fog + step < c->target ? c->fog + step : c->target;
        }
    }
}

static void SoftViewportUpdate(NodeGuardedViewport* self) {
    if (sActive) {
        Ease();
    }
    sViewportUpdate(self);
}

// Tracking from nothing: every cell starts fogged and fades in.
static void Start(void) {
    memset(sCells, 0, sizeof(sCells));
    sTick = 0;
    sLastNs = 0;
    sActive = 1;
    sFade = Psyz_GteSetDepthCueHook(DepthCue);
}

static void Stop(void) {
    sActive = 0;
    sShownMap = NULL; // what it showed goes with the map, or SoftFog_Set hid it
    sShownCount = 0;
    Psyz_GteSetDepthCueHook(NULL);
}

static void SoftDayTaskOnInit(DayTask* self, s32 a, s32 b, s32 c) {
    sDayTaskOnInit(self, a, b, c);
    sInDream = 1;
    sEnabled = sNextEnabled;
    if (sEnabled) {
        Start();
    }
}

static void SoftDayTaskOnDeinit(DayTask* self) {
    sInDream = 0;
    Stop();
    sDayTaskOnDeinit(self);
}

void SoftFog_Set(int on) {
    sNextEnabled = on;
    if (sInDream && on != sEnabled) {
        // On: from the next tick, the view faded in from the fog. Off: at once.
        sEnabled = on;
        if (on) {
            Start();
        } else {
            // the cells it showed hidden; the game shows its window's again
            // at its next refresh
            if (sShownMap != NULL) {
                HideShown(sShownMap);
            }
            Stop();
        }
    }
}

void SoftFog_Init(int on) {
    sNextEnabled = on;
    sRefreshFootprint = gStageMapMethods.refreshFootprint;
    gStageMapMethods.refreshFootprint = SoftRefreshFootprint;
    sViewportUpdate = (void*)gNodeGuardedViewportMethods.update;
    gNodeGuardedViewportMethods.update = (void*)SoftViewportUpdate;
    sDayTaskOnInit = gDayTaskMethods.onInit;
    sDayTaskOnDeinit = gDayTaskMethods.onDeinit;
    gDayTaskMethods.onInit = SoftDayTaskOnInit;
    gDayTaskMethods.onDeinit = SoftDayTaskOnDeinit;
}
