// The far map: the dream's map past the seven chunks the StageMap keeps
// loaded, loaded and drawn by the port, and the cells shown past the game's
// window, for a view wider (widescreen) or deeper (draw_distance) than the
// console's.
//
// The StageMap keeps the player's chunk and its six neighbours loaded (a
// chunk is 20 x 20 cells; odd rows of chunks sit half a chunk to -x) and
// draws a window of 20 x 20 cells ahead of the player from them
// (StageMap__ComputeFootprintFromRotation), hiding every other cell with
// GsDOFF. Past that window it draws nothing, and past the seven chunks it
// has nothing to draw.
//
// The far chunks: every chunk within LOAD_CELLS of the player that no slot
// of the game's holds is read here, from the file the game would read for
// it (the StageMap's chunkFileFn gives its record, whose disc position the
// game found at boot), through a handle of the port's own on the disc
// image (src/disc.c), so the game's CD driver never sees it. Its header
// block (placements and models, LBDFILE_HEADER_BLOCK_SIZE bytes) is
// turned into cells as StageMap__PopulateSlotCells does, in memory of the
// port's, not the game's pool: GridCells with GridCell's method table, so
// Viewport__DrawNode draws them and the code that walks the game's cells
// (below, src/soft_fog.c) takes them as they are, but in no list of the
// game's: no StageMap slot, no scene node's children. The data block (the
// chunk's objects, which the game spawns from STAGEMAP_EVENT_SLOT_DATA_READY)
// is not read. A far chunk sits where the game's grid puts it, relative to
// the game's centre slot, under that slot's coordinate; when a slot of the
// game's has populated the same chunk, the far one steps aside. The chunks
// are kept (FAR_CACHE of them, the least recently wanted dropped) while
// their stage lasts.
//
// The cells shown past the window, at each refresh of the game's window
// (refreshFootprint, wrapped first, so that src/soft_fog.c's wrapper sees
// them): with the console's fog, every cell, of the game's chunks or the far
// ones, in the view cone (this tick's yaw and the last one's, for smooth's
// in-between frames) and either within the window's rows (or columns) ahead,
// as the window's horizon, or, with a draw distance past 1, within
// FarMap_Reach cells. Soft fog shapes the view itself (ShapeView), from the
// same chunks. What is shown here is hidden again before the next refresh.
//
// Drawing: the Viewport's update (wrapped) draws the shown far cells after
// the game's draw, through its drawNode, into the same ordering table but
// a packet area of the port's, and puts back what that draw leaves behind
// that the game could read: the GTE's registers, the scratchpad, libgs's
// packet pointer and the renderer's per-object state. Their scale is the
// game's cells' (the StageMap's scale ramp, blended by src/pacing.c), and
// their attribute the game's cells' but for GsDOFF.
// The dream's Viewport gets a larger packet area of the port's for the
// game's own cells, shown past the window (FarDayTaskOnInit).
//
// Only drawing: the StageMap's slots, window, commands and tracking are the
// game's own; the commands it hands cells (DispatchToRectCells) take their
// own 3 x 3 window around the sender and the game's slots. Nothing is on
// at 4:3, draw distance 1 and the console's fog, nor on stages whose chunks
// are stacked in a column (StageMap::config->isVertical).

#include "common.h"
#include "cd_driver.h"
#include "day_task.h"
#include "grid_cell.h"
#include "lbd_file.h"
#include "node_guarded_viewport.h"
#include "placement_grid.h"
#include "stage_map.h"
#include "tmd_model.h"
#include "viewport.h"

#include "disc.h"
#include "draw_distance.h"
#include "far_map_cells.h"
#include "soft_fog.h"
#include "widescreen.h"

#include <libetc.h>
#include <libgpu.h>
#include <libgs.h>
#include <math.h>
#include <psyz/gte.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The window the game draws reaches this many cells ahead.
#define WINDOW_CELLS 20
// The furthest the view reaches with a draw distance past 1: the cells'
// depth stays inside the GTE's 16-bit Z (65535, 32 cells).
#define FAR_CELLS 30
// Chunks with a cell this near the player are loaded: the soft fog's circle
// (FarMap_Reach + 1), and the far corners of the window's rows ahead in the
// 16:9 cone, up to 37 cells away looking diagonally (a model over positions
// and headings: within 33 cells, 4 % of them still showed a gap).
#define LOAD_CELLS 37
// Far chunks kept.
#define FAR_CACHE 40
// The packet area for the far cells, each half, and how much of it must be
// left before a cell is drawn.
#define PACKET_BYTES (6 << 20)
#define PACKET_MARGIN (512 << 10)
// The packet area the dream's Viewport gets instead of its own, each half
// (below, FarDayTaskOnInit).
#define GAME_PACKET_BYTES (4 << 20)

// A far cell: what the draw and the cell walks see, with its coordinate.
typedef struct {
    GridCell cell;
    GsCOORDINATE2 coord;
    GsCOORD2PARAM param;
} FarCell;

typedef struct {
    const CdFileEntry* file; // the chunk's record, its key; NULL: free
    s32 chunk;               // its index in the stage's grid
    u32 wanted;              // the last refresh that wanted it
    GteLong origin[3];       // its corner in the world
    GsCOORDINATE2 coord;     // the same, under the game's centre slot
    u8* data;                // the file's header block
    FarCell* cells;          // the lattice's cells, then the chained ones
    int cellCount;
    GridCell* lattice[STAGE_SLOT_LATTICE_CELLS];
} FarChunk;

static Disc* sDisc;
static FarChunk sFar[FAR_CACHE];
static u32 sRefresh; // refreshes so far

static StageMap* sMap;      // the StageMap the far chunks are for
static ChunkSlot* sCentre;  // its centre slot at the last refresh
static int sNear[FAR_CACHE]; // the far chunks in use: wanted, held by no slot
static int sNearCount;

static int sInDream;
static Viewport* sViewport; // the DayTask's
static PACKET* sPackets[2];
static PACKET* sGamePackets[2];
static PACKET* sGameWorkBase[2]; // the Viewport's own, to give back

static int sStats; // LSD_FAR_MAP_STATS: loads and packets on stderr
static u64 sLoadNs, sLoadMaxNs;
static int sLoads;
static size_t sPacketPeak;
static int sPacketFull;
static u64 sDrawNs, sRefreshNs; // in DrawFar and in the refresh, since the last report
static int sDraws, sRefreshes;

static void (*sRefreshFootprint)(StageMap* self);
static void (*sViewportUpdate)(NodeGuardedViewport* self);
static void (*sDayTaskOnInit)(DayTask* self, s32 a, s32 b, s32 c);
static void (*sDayTaskOnDeinit)(DayTask* self);

// --- loading ---

static int Bcd(u_char v) {
    return (v >> 4) * 10 + (v & 15);
}

static void FreeChunk(FarChunk* fc) {
    SDL_free(fc->data);
    SDL_free(fc->cells);
    memset(fc, 0, sizeof(*fc));
}

static void InitCell(FarChunk* fc, FarCell* c) {
    c->cell.methods = &gGridCellMethods;
    c->cell.coord2 = &c->coord;
    c->cell.attribute = GsDOFF;
    c->coord.param = &c->param;
    c->coord.super = &fc->coord;
    c->param.scale.vx = c->param.scale.vy = c->param.scale.vz = ONE;
}

// A placement record into a cell, as PlacementGrid__ResolveEntry and
// StageMap__PopulateSlotCells do it.
static void PlaceCell(FarCell* c, const PlacementGridRecord* rec, int index, TmdFile* tmd) {
    TmdObject* object = &tmd->objects[rec->model];
    c->coord.coord.t[0] = (index % STAGE_CHUNK_CELLS << STAGE_CELL_SHIFT) + STAGE_CELL_SIZE / 2;
    c->coord.coord.t[1] = (s32)rec->y << STAGE_CELL_SHIFT;
    c->coord.coord.t[2] = (index / STAGE_CHUNK_CELLS << STAGE_CELL_SHIFT) + STAGE_CELL_SIZE / 2;
    c->param.rotate.vy = rec->rotY * ANGLE_DEG(90);
    c->cell.flags36 = rec->cellFlags;
    c->cell.model = object;
    GsLinkObject4((u_long)object, (GsDOBJ2*)&c->cell.attribute, 0);
}

// The chunk's cells from its header block; 0, or -1 if the block is not
// what it should be.
static int BuildCells(FarChunk* fc, size_t size) {
    const LbdFileHeader* header = (const LbdFileHeader*)fc->data;
    u8* placements = fc->data + header->placementsOffset;
    TmdFile* tmd;
    size_t tmdAt = (size_t)header->placementsOffset + header->placementsSize;
    int count = STAGE_SLOT_LATTICE_CELLS;
    int pass, n, i;

    if (header->placementsOffset < (s32)sizeof(LbdFileHeader) || header->placementsSize <= 0 ||
        (size_t)header->placementsOffset +
                STAGE_SLOT_LATTICE_CELLS * sizeof(PlacementGridRecord) > size ||
        tmdAt + offsetof(TmdFile, objects) > size) {
        return -1;
    }
    tmd = (TmdFile*)(fc->data + tmdAt);
    if (tmdAt + offsetof(TmdFile, objects) + tmd->nobj * sizeof(TmdObject) > size) {
        return -1;
    }
    GsMapModelingData((u_long*)&tmd->flags);

    // Twice: count the chained records, then fill the cells.
    for (pass = 0; pass < 2; pass++) {
        n = STAGE_SLOT_LATTICE_CELLS;
        for (i = 0; i < STAGE_SLOT_LATTICE_CELLS; i++) {
            const PlacementGridRecord* rec = (const PlacementGridRecord*)placements + i;
            FarCell* prev = pass ? &fc->cells[i] : NULL;
            s32 next;
            int guard = 0;
            if (pass) {
                InitCell(fc, prev);
                fc->lattice[i] = &prev->cell;
                if (rec->present && rec->model < tmd->nobj) {
                    PlaceCell(prev, rec, i, tmd);
                }
            }
            for (next = rec->next; next != 0 && guard < 64; guard++) {
                const PlacementGridRecord* more =
                    (const PlacementGridRecord*)(placements + next);
                if (next < 0 || (size_t)header->placementsOffset + next + sizeof(*more) > size) {
                    break;
                }
                if (more->present && more->model < tmd->nobj) {
                    if (pass) {
                        FarCell* c = &fc->cells[n];
                        InitCell(fc, c);
                        PlaceCell(c, more, i, tmd);
                        prev->cell.nextInCell = &c->cell;
                        prev = c;
                    }
                    n++;
                }
                next = more->next;
            }
        }
        if (!pass) {
            count = n;
            fc->cells = SDL_calloc((size_t)count, sizeof(FarCell));
            if (fc->cells == NULL) {
                return -1;
            }
        }
    }
    fc->cellCount = count;
    return 0;
}

// Reads chunk `chunk`'s file into a free (or the least recently wanted)
// entry; NULL if there is none or the file can't be read.
static FarChunk* LoadChunk(const CdFileEntry* file, s32 chunk) {
    FarChunk* fc = NULL;
    u64 start = SDL_GetTicksNS();
    size_t size = file->size < LBDFILE_HEADER_BLOCK_SIZE ? file->size : LBDFILE_HEADER_BLOCK_SIZE;
    int lba = (Bcd(file->pos.minute) * 60 + Bcd(file->pos.second)) * 75 +
              Bcd(file->pos.sector) - 150;
    int i;

    if (sDisc == NULL || size < sizeof(LbdFileHeader) || lba < 0) {
        return NULL;
    }
    for (i = 0; i < FAR_CACHE; i++) {
        if (sFar[i].file == NULL) {
            fc = &sFar[i];
            break;
        }
        if (sFar[i].wanted != sRefresh && (fc == NULL || sFar[i].wanted < fc->wanted)) {
            fc = &sFar[i];
        }
    }
    if (fc == NULL) {
        return NULL;
    }
    FreeChunk(fc);
    fc->data = SDL_malloc(size);
    if (fc->data == NULL || Disc_Read(sDisc, (unsigned)lba, fc->data, size) != 0 ||
        BuildCells(fc, size) != 0) {
        FreeChunk(fc);
        return NULL;
    }
    fc->file = file;
    fc->chunk = chunk;
    if (sStats) {
        u64 ns = SDL_GetTicksNS() - start;
        sLoads++;
        sLoadNs += ns;
        sLoadMaxNs = ns > sLoadMaxNs ? ns : sLoadMaxNs;
    }
    return fc;
}

static void HideFarCells(void) {
    int i, k;
    for (i = 0; i < FAR_CACHE; i++) {
        for (k = 0; k < sFar[i].cellCount; k++) {
            sFar[i].cells[k].cell.attribute |= GsDOFF;
        }
    }
}

static ChunkSlot* CentreSlot(StageMap* self) {
    int i;
    for (i = 0; i < CHUNK_NEIGHBOUR_COUNT; i++) {
        if (self->slots[i].loader->elemKey == CHUNK_NEIGHBOUR_CENTRE) {
            return &self->slots[i];
        }
    }
    return NULL;
}

// Whether a slot of the game's has chunk `chunk` in its cells.
static int HeldByGame(StageMap* self, s32 chunk) {
    int i;
    for (i = 0; i < CHUNK_NEIGHBOUR_COUNT; i++) {
        LbdFile* loader = self->slots[i].loader;
        if (loader->chunkIndex == chunk && loader->headerReady == LBDFILE_HEADER_CONSUMED &&
            !self->slots[i].loadPending) {
            return 1;
        }
    }
    return 0;
}

// The far chunks near the player, loaded, placed and listed in sNear.
static void UpdateChunks(StageMap* self) {
    StageGridDimensions* dims = self->config;
    ChunkSlot* centre = CentreSlot(self);
    GteLong* pos = self->target->coord2->coord.t;
    const GteLong* co;
    s32 cc, cr, c, r;
    int i;

    sNearCount = 0;
    sCentre = centre;
    if (centre == NULL || centre->loader->chunkIndex < 0 || dims->columns <= 0) {
        return;
    }
    co = centre->cellParent->coord2->coord.t;
    cc = centre->loader->chunkIndex % dims->columns;
    cr = centre->loader->chunkIndex / dims->columns;
    for (r = cr - 3; r <= cr + 3; r++) {
        for (c = cc - 3; c <= cc + 3; c++) {
            s32 chunk = r * dims->columns + c;
            s32 dx, dz, ox, oz;
            const CdFileEntry* file;
            FarChunk* fc = NULL;
            if (r < 0 || r >= dims->rows || c < 0 || c >= dims->columns) {
                continue;
            }
            // Its corner from the centre's (ComputeCellWorldOffsets: odd
            // rows half a chunk to -x).
            dx = (c - cc) * STAGE_CHUNK_SIZE + ((cr & 1) - (r & 1)) * (STAGE_CHUNK_SIZE / 2);
            dz = (r - cr) * STAGE_CHUNK_SIZE;
            ox = co[0] + dx - pos[0];
            oz = co[2] + dz - pos[2];
            ox = ox > 0 ? ox : (ox + STAGE_CHUNK_SIZE < 0 ? -(ox + STAGE_CHUNK_SIZE) : 0);
            oz = oz > 0 ? oz : (oz + STAGE_CHUNK_SIZE < 0 ? -(oz + STAGE_CHUNK_SIZE) : 0);
            if ((float)ox * ox + (float)oz * oz >
                    (float)(LOAD_CELLS * STAGE_CELL_SIZE) * (LOAD_CELLS * STAGE_CELL_SIZE) ||
                HeldByGame(self, chunk)) {
                continue;
            }
            file = self->chunkFileFn(self->chunkFileCtx, chunk, 0, 0);
            if (file == NULL) {
                continue;
            }
            for (i = 0; i < FAR_CACHE; i++) {
                if (sFar[i].file == file && sFar[i].chunk == chunk) {
                    fc = &sFar[i];
                    break;
                }
            }
            if (fc == NULL) {
                fc = LoadChunk(file, chunk);
            }
            if (fc == NULL) {
                continue;
            }
            fc->wanted = sRefresh;
            fc->origin[0] = co[0] + dx;
            fc->origin[1] = co[1];
            fc->origin[2] = co[2] + dz;
            fc->coord.coord = GsIDMATRIX;
            fc->coord.coord.t[0] = dx;
            fc->coord.coord.t[1] = 0;
            fc->coord.coord.t[2] = dz;
            fc->coord.super = centre->cellParent->coord2;
            fc->coord.flg = 0;
            sNear[sNearCount++] = (int)(fc - sFar);
        }
    }
}

int FarMap_Chunks(StageMap* self, MapChunk* out) {
    int n = 0, i;
    if (self->config == NULL || self->config->isVertical != 0) {
        return 0;
    }
    for (i = 0; i < CHUNK_NEIGHBOUR_COUNT; i++) {
        ChunkSlot* slot = &self->slots[i];
        if (slot->loader->headerReady == 0 || slot->loader->chunkIndex < 0) {
            continue;
        }
        out[n].id = i;
        out[n].chunk = slot->loader->chunkIndex;
        out[n].origin = slot->cellParent->coord2->coord.t;
        out[n].cells = slot->cells;
        n++;
    }
    for (i = 0; self == sMap && i < sNearCount; i++) {
        FarChunk* fc = &sFar[sNear[i]];
        out[n].id = FAR_MAP_FIRST_ID + sNear[i];
        out[n].chunk = fc->chunk;
        out[n].origin = fc->origin;
        out[n].cells = fc->lattice;
        n++;
    }
    return n;
}

GridCell** FarMap_Cells(StageMap* self, int id, s32 chunk) {
    if (id < FAR_MAP_FIRST_ID) {
        ChunkSlot* slot = &self->slots[id];
        return slot->loader->headerReady != 0 && slot->loader->chunkIndex == chunk ? slot->cells
                                                                                   : NULL;
    }
    id -= FAR_MAP_FIRST_ID;
    return self == sMap && sFar[id].file != NULL && sFar[id].chunk == chunk ? sFar[id].lattice
                                                                            : NULL;
}

int FarMap_Reach(void) {
    s32 fogNear = DrawDistance_FogNear();
    int cells;
    if (DrawDistance_Scale() <= 1 || fogNear <= 0) {
        return WINDOW_CELLS;
    }
    // The fog culls at five times its near distance (SetFogNear).
    cells = (int)((5.0f * fogNear) / STAGE_CELL_SIZE);
    return cells > FAR_CELLS ? FAR_CELLS : (cells < WINDOW_CELLS ? WINDOW_CELLS : cells);
}

// --- the cells shown past the window ---

// A cell shown here, to hide at the next refresh: its chunk's id, the chunk
// it held then, and the cell's lattice index.
typedef struct {
    s16 id;
    s16 chunk;
    s16 cell;
} ExtraCell;

static StageMap* sExtraMap;
static int sExtraCount;
static ExtraCell sExtras[FAR_MAP_MAX_CHUNKS * STAGE_SLOT_LATTICE_CELLS];
static s16 sPrevYaw; // the target's yaw at the last refresh
static MapChunk sChunks[FAR_MAP_MAX_CHUNKS];

// The view's half-width over its depth: the projection's 160 / 266 at 4:3.
#define HALF_WIDTH_4_3 (160.0f / 266.0f)
// How far a cell may lie outside the cone and still be shown: its half
// diagonal, and a cell more for models that reach past their cell.
#define CONE_MARGIN (1448.0f + STAGE_CELL_SIZE)

static void SetCellShown(GridCell* cell, int shown) {
    for (; cell != NULL; cell = cell->nextInCell) {
        if (shown) {
            cell->attribute &= ~GsDOFF;
        } else {
            cell->attribute |= GsDOFF;
        }
    }
}

static void HideExtras(StageMap* self) {
    int i;
    if (self == sExtraMap) {
        for (i = 0; i < sExtraCount; i++) {
            GridCell** cells = FarMap_Cells(self, sExtras[i].id, sExtras[i].chunk);
            if (cells != NULL) {
                SetCellShown(cells[sExtras[i].cell], 0);
            }
        }
    }
    sExtraMap = self;
    sExtraCount = 0;
}

// Whether a point (dx, dz from the camera) lies in the view cone of a yaw
// (forward fx, fz), give or take CONE_MARGIN.
static int InCone(float dx, float dz, float fx, float fz, float slope, float margin) {
    float along = dx * fx + dz * fz;
    float across = fabsf(dx * fz - dz * fx);
    return along >= -CONE_MARGIN && across <= along * slope + margin;
}

static void ShowExtras(StageMap* self, float stretch, int reach) {
    GsCOORD2PARAM* param = self->target->coord2->param;
    GteLong* pos = self->target->coord2->coord.t;
    // The cone at this tick's yaw and at the last one's: src/pacing.c's
    // in-between frames turn the camera from one to the other.
    float yaw = param->rotate.vy * (6.2831853f / ONE);
    float prev = sPrevYaw * (6.2831853f / ONE);
    float fx = sinf(yaw), fz = cosf(yaw);
    float px = sinf(prev), pz = cosf(prev);
    float slope = HALF_WIDTH_4_3 * stretch;
    float margin = CONE_MARGIN * sqrtf(1.0f + slope * slope);
    float far2 = (float)(reach * STAGE_CELL_SIZE) * (reach * STAGE_CELL_SIZE);
    u16 angle = (u16)param->rotate.vy & (ONE - 1);
    // The window's ahead axis, by ComputeFootprintFromRotation's own test.
    int aheadX = (u16)(angle - ANGLE_DEG(45)) < ANGLE_DEG(90) ||
                 (u16)(angle - ANGLE_DEG(225)) < ANGLE_DEG(90);
    s32 lo = 0x7FFFFFFF, hi = -0x7FFFFFFF;
    int count = FarMap_Chunks(self, sChunks);
    int i, k;

    // The window's extent along that axis, in world units.
    for (i = 0; i < self->rectCount; i++) {
        CellRect* r = &self->rects.e[i];
        GteLong* o = self->slots[r->slotIndex].cellParent->coord2->coord.t;
        s32 a = aheadX ? o[0] + r->col * STAGE_CELL_SIZE : o[2] + r->row * STAGE_CELL_SIZE;
        s32 b = a + (aheadX ? r->width : r->height) * STAGE_CELL_SIZE;
        if (self->slots[r->slotIndex].loader->headerReady == 0) {
            continue;
        }
        if (a < lo) {
            lo = a;
        }
        if (b > hi) {
            hi = b;
        }
    }

    for (i = 0; i < count; i++) {
        const GteLong* o = sChunks[i].origin;
        for (k = 0; k < STAGE_SLOT_LATTICE_CELLS; k++) {
            GridCell* cell = sChunks[i].cells[k];
            s32 x = o[0] + (k % STAGE_CHUNK_CELLS) * STAGE_CELL_SIZE;
            s32 z = o[2] + (k / STAGE_CHUNK_CELLS) * STAGE_CELL_SIZE;
            s32 ahead = aheadX ? x : z;
            float dx, dz;
            if (!(cell->attribute & GsDOFF) || (cell->model == NULL && cell->nextInCell == NULL)) {
                continue; // shown by the game already, or empty
            }
            dx = (float)(x + STAGE_CELL_SIZE / 2 - pos[0]);
            dz = (float)(z + STAGE_CELL_SIZE / 2 - pos[2]);
            // the window's horizon, or past a draw distance of 1, a circle
            if ((ahead < lo || ahead >= hi) &&
                (reach <= WINDOW_CELLS || dx * dx + dz * dz > far2)) {
                continue;
            }
            if (!InCone(dx, dz, fx, fz, slope, margin) && !InCone(dx, dz, px, pz, slope, margin)) {
                continue;
            }
            SetCellShown(cell, 1);
            sExtras[sExtraCount].id = (s16)sChunks[i].id;
            sExtras[sExtraCount].chunk = (s16)sChunks[i].chunk;
            sExtras[sExtraCount].cell = (s16)k;
            sExtraCount++;
        }
    }
}

static void PrintStats(void) {
    fprintf(stderr,
            "far map: %d far chunks in use; %d loads, %.2f ms on average, %.2f ms at most; "
            "packets %zu KB at most, full %d times; %.0f us a refresh, %.0f us a draw\n",
            sNearCount, sLoads, sLoads ? sLoadNs / 1e6 / sLoads : 0.0, sLoadMaxNs / 1e6,
            sPacketPeak >> 10, sPacketFull, sRefreshes ? sRefreshNs / 1e3 / sRefreshes : 0.0,
            sDraws ? sDrawNs / 1e3 / sDraws : 0.0);
    sDrawNs = sRefreshNs = 0;
    sDraws = sRefreshes = 0;
}

static void FarRefreshFootprint(StageMap* self) {
    u64 start = SDL_GetTicksNS();
    int same = self == sExtraMap;
    float stretch = Widescreen_Stretch();
    int reach = FarMap_Reach();
    HideExtras(self);
    HideFarCells();
    sRefreshFootprint(self);
    if (self != sMap) {
        sMap = self;
        sNearCount = 0;
    }
    sRefresh++;
    if (!sInDream || self->chunksLoaded == 0 || self->config->isVertical != 0) {
        sNearCount = 0;
        return;
    }
    if (stretch > 1.0f || reach > WINDOW_CELLS || SoftFog_On()) {
        UpdateChunks(self);
    } else {
        sNearCount = 0;
    }
    if (sStats && sRefresh % 100 == 0) {
        PrintStats();
    }
    if (stretch > 1.0f || reach > WINDOW_CELLS) {
        if (!same) {
            sPrevYaw = self->target->coord2->param->rotate.vy;
        }
        ShowExtras(self, stretch, reach);
    }
    sPrevYaw = self->target->coord2->param->rotate.vy;
    if (sStats) {
        sRefreshNs += SDL_GetTicksNS() - start;
        sRefreshes++;
    }
}

// --- drawing ---

typedef struct {
    unsigned data[32], ctrl[32];
    unsigned char scratch[1024];
    PACKET* packet;
    s32 lightOff, ndiv, useGlobalLightMode, lightMode;
} DrawState;

extern s32 sSortLightOff;
extern s32 sSortNdiv;
extern s32 sSortUseGlobalLightMode;
extern s32 sSortLightMode;

static void SaveDrawState(DrawState* s) {
    unsigned i;
    for (i = 0; i < 32; i++) {
        s->data[i] = Psyz_GteDataRead(i);
        s->ctrl[i] = Psyz_GteCtrlRead(i);
    }
    memcpy(s->scratch, getScratchAddr(0), sizeof(s->scratch));
    s->packet = GsOUT_PACKET_P;
    s->lightOff = sSortLightOff;
    s->ndiv = sSortNdiv;
    s->useGlobalLightMode = sSortUseGlobalLightMode;
    s->lightMode = sSortLightMode;
}

static void RestoreDrawState(const DrawState* s) {
    unsigned i;
    for (i = 0; i < 32; i++) {
        Psyz_GteCtrlWrite(i, s->ctrl[i]);
    }
    for (i = 0; i < 32; i++) {
        // not SXYP (it pushes the screen-XY FIFO), IRGB (it sets IR1-3),
        // ORGB or LZCR (read-only)
        if (i != 15 && i != 28 && i != 29 && i != 31) {
            Psyz_GteDataWrite(i, s->data[i]);
        }
    }
    memcpy(getScratchAddr(0), s->scratch, sizeof(s->scratch));
    GsOUT_PACKET_P = s->packet;
    sSortLightOff = s->lightOff;
    sSortNdiv = s->ndiv;
    sSortUseGlobalLightMode = s->useGlobalLightMode;
    sSortLightMode = s->lightMode;
}

// Whether the Viewport's update drew the StageMap: it hangs off one of the
// roots it draws.
static int MapDrawn(Viewport* vp, StageMap* map) {
    SceneNode* root = GetRootNode((SceneNode*)map);
    return vp->otReady != 0 &&
           (root == vp->sceneRoot || (vp->viewNode != NULL && root == GetRootNode(vp->viewNode)));
}

static void DrawFar(Viewport* vp) {
    static DrawState saved;
    GridCell* model = sCentre->cells[0]; // the game's cells' attribute and scale
    u_long attribute = model->attribute & ~GsDOFF;
    VECTOR scale = model->coord2->param->scale;
    u8* base = (u8*)sPackets[vp->otIndex];
    int i, k;

    SaveDrawState(&saved);
    GsOUT_PACKET_P = (PACKET*)base;
    for (i = 0; i < sNearCount; i++) {
        FarChunk* fc = &sFar[sNear[i]];
        fc->coord.flg = 0;
        for (k = 0; k < fc->cellCount; k++) {
            FarCell* c = &fc->cells[k];
            if ((c->cell.attribute & GsDOFF) || c->cell.tmd == 0) {
                continue;
            }
            if ((u8*)GsOUT_PACKET_P - base > PACKET_BYTES - PACKET_MARGIN) {
                sPacketFull++;
                i = sNearCount;
                break;
            }
            c->cell.attribute = attribute;
            c->param.scale = scale;
            c->coord.flg = 0;
            vp->methods->drawNode(vp, (SceneNode*)&c->cell);
        }
    }
    if ((size_t)((u8*)GsOUT_PACKET_P - base) > sPacketPeak) {
        sPacketPeak = (size_t)((u8*)GsOUT_PACKET_P - base);
    }
    RestoreDrawState(&saved);
}

static void FarViewportUpdate(NodeGuardedViewport* self) {
    Viewport* vp = (Viewport*)self;
    sViewportUpdate(self);
    if (vp == sViewport && sMap != NULL && sNearCount > 0 && sCentre != NULL &&
        sPackets[0] != NULL && MapDrawn(vp, sMap)) {
        u64 start = SDL_GetTicksNS();
        DrawFar(vp);
        if (sStats) {
            sDrawNs += SDL_GetTicksNS() - start;
            sDraws++;
        }
    }
}

// The dream's Viewport draws into a packet area of 1200 packets of 64
// bytes (twice that on a 64-bit host), which the game's own window keeps
// well within; what the port shows past the window (the game's cells
// beyond it, here and in src/soft_fog.c) goes there too, and on the busiest
// stages took it to two thirds. Nothing checks for its end, so it gets one
// of GAME_PACKET_BYTES instead: libgs reads Viewport::workBase in update
// only, and nothing of the game's reads the packets but the GPU.
static void FarDayTaskOnInit(DayTask* self, s32 a, s32 b, s32 c) {
    sDayTaskOnInit(self, a, b, c);
    sInDream = 1;
    sViewport = (Viewport*)self->viewport;
    if (sViewport->otReady != 0 && sGamePackets[0] != NULL) {
        sGameWorkBase[0] = sViewport->workBase[0];
        sGameWorkBase[1] = sViewport->workBase[1];
        sViewport->workBase[0] = sGamePackets[0];
        sViewport->workBase[1] = sGamePackets[1];
    }
}

static void FarDayTaskOnDeinit(DayTask* self) {
    if (sViewport != NULL && sGameWorkBase[0] != NULL) {
        DrawSync(0); // the GPU done with the packets
        sViewport->workBase[0] = sGameWorkBase[0];
        sViewport->workBase[1] = sGameWorkBase[1];
        sGameWorkBase[0] = sGameWorkBase[1] = NULL;
    }
    sInDream = 0;
    sViewport = NULL;
    sMap = NULL; // the StageMap goes with the DayTask
    sNearCount = 0;
    sExtraMap = NULL;
    sExtraCount = 0;
    if (sStats) {
        PrintStats();
    }
    sDayTaskOnDeinit(self);
}

void FarMap_Init(const char* discPath) {
    sDisc = Disc_Open(discPath);
    sPackets[0] = SDL_malloc(2 * (size_t)PACKET_BYTES);
    sPackets[1] = sPackets[0] != NULL ? (PACKET*)((u8*)sPackets[0] + PACKET_BYTES) : NULL;
    sGamePackets[0] = SDL_malloc(2 * (size_t)GAME_PACKET_BYTES);
    sGamePackets[1] =
        sGamePackets[0] != NULL ? (PACKET*)((u8*)sGamePackets[0] + GAME_PACKET_BYTES) : NULL;
    sStats = SDL_getenv("LSD_FAR_MAP_STATS") != NULL;
    sRefreshFootprint = gStageMapMethods.refreshFootprint;
    gStageMapMethods.refreshFootprint = FarRefreshFootprint;
    sViewportUpdate = (void*)gNodeGuardedViewportMethods.update;
    gNodeGuardedViewportMethods.update = (void*)FarViewportUpdate;
    sDayTaskOnInit = gDayTaskMethods.onInit;
    sDayTaskOnDeinit = gDayTaskMethods.onDeinit;
    gDayTaskMethods.onInit = FarDayTaskOnInit;
    gDayTaskMethods.onDeinit = FarDayTaskOnDeinit;
}
