// Widescreen: the dream drawn with a wider field of view, shown at a wider
// aspect ratio; everything else (title menu, graph, movies, images) stays
// 4:3 with bars at the sides.
//
// Anamorphic, so the PS1's VRAM layout stays as it is: psyz's GTE squeezes
// every projected X around the screen centre (Psyz_GteSetScreenXScale) into
// the 320x240 framebuffer, and the display stretches it back
// (Psyz_VideoSetDisplayStretch). Both are on only while a DayTask, which
// runs one day of the dream, is initialised: its onInit and onDeinit slots
// are wrapped here, so the game's C stays as lsddecomp has it.
//
// The map's footprint is widened sideways as well. The StageMap draws a
// window of 20 x 20 cells ahead of the player, axis-aligned and shifted
// toward the look direction (StageMap__ComputeFootprintFromRotation); it
// covers the console's view, but the wider one runs out of it at the side
// the window was shifted away from, as near as 5 cells (10000 units) ahead. Its
// refreshFootprint slot is wrapped: after the game's window is shown, every
// loaded cell in the wider view cone and within the window's rows (or
// columns) ahead is shown too, and hidden again before the next refresh.
// The window is only drawing (GsDOFF on the cells); the commands StageMap
// hands to cells use their own window around the sender.
//
// World sprites: Viewport__DrawNode projects a world sprite itself and
// hands GsSortSprite the screen position, which psyz's GTE path squeezes,
// except for a plain sprite (scale 1, no rotation): that one is a SPRT,
// drawn where it is, 4/3 too far from the centre. The only plain ones in a
// dream are StyleEffect's VariantSprites (all five in its plain kind, the
// first in the jitter kind), so VariantSprite's reset (the ctor's last
// step) and updateScale are wrapped to leave scalex at ONE + 1 instead of
// ONE: the GTE path, like their siblings, a 1/4096 larger.
//
// Built with the game's C (it needs DayTask's, StageMap's and
// VariantSprite's method tables), not with the port's other files.

#include "common.h"
#include "day_task.h"
#include "grid_cell.h"
#include "lbd_file.h"
#include "stage_map.h"
#include "variant_sprite.h"
#include "widescreen.h"

#include <math.h>
#include <psyz/gte.h>
#include <psyz/video.h>
#include <stdio.h>

static int sScaleX = 0x10000; // the GTE's X scale in the dream, 16.16
static float sStretch = 1.0f; // the display's stretch in the dream
static int sWide;             // 1 while a DayTask is initialised
static int sExtraCount;       // how many of sExtras (below) are shown

// onInit's slot type: IntermediateBase__Init calls it as onInit(self, 0, 0, 0).
static void (*sDayTaskOnInit)(DayTask* self, s32 a, s32 b, s32 c);
static void (*sDayTaskOnDeinit)(DayTask* self);

static void EnterWide(void) {
    sWide = 1;
    Psyz_GteSetScreenXScale(sScaleX);
    Psyz_VideoSetDisplayStretch(sStretch);
}

static void LeaveWide(void) {
    sWide = 0;
    sExtraCount = 0; // the StageMap goes with the DayTask
    Psyz_GteSetScreenXScale(0x10000);
    Psyz_VideoSetDisplayStretch(1.0f);
}

// The cells shown beyond the game's window, to hide at the next refresh: a
// slot, the chunk it held then, and the cell's lattice index.
typedef struct {
    s16 slot;
    s16 chunk;
    s16 cell;
} ExtraCell;

static void (*sRefreshFootprint)(StageMap* self);
static StageMap* sExtraMap;
static s16 sPrevYaw; // the target's yaw at the last refresh
static ExtraCell sExtras[CHUNK_NEIGHBOUR_COUNT * STAGE_SLOT_LATTICE_CELLS];

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
            ChunkSlot* slot = &self->slots[sExtras[i].slot];
            if (slot->loader->headerReady != 0 &&
                slot->loader->chunkIndex == sExtras[i].chunk) {
                SetCellShown(slot->cells[sExtras[i].cell], 0);
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

static void ShowExtras(StageMap* self) {
    GsCOORD2PARAM* param = self->target->coord2->param;
    GteLong* pos = self->target->coord2->coord.t;
    // The cone at this tick's yaw and at the last one's: src/pacing.c's
    // in-between frames turn the camera from one to the other.
    float yaw = param->rotate.vy * (6.2831853f / ONE);
    float prev = sPrevYaw * (6.2831853f / ONE);
    float fx = sinf(yaw), fz = cosf(yaw);
    float px = sinf(prev), pz = cosf(prev);
    float slope = HALF_WIDTH_4_3 * sStretch;
    float margin = CONE_MARGIN * sqrtf(1.0f + slope * slope);
    u16 angle = (u16)param->rotate.vy & (ONE - 1);
    // The window's ahead axis, by ComputeFootprintFromRotation's own test.
    int aheadX = (u16)(angle - ANGLE_DEG(45)) < ANGLE_DEG(90) ||
                 (u16)(angle - ANGLE_DEG(225)) < ANGLE_DEG(90);
    s32 lo = 0x7FFFFFFF, hi = -0x7FFFFFFF;
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
    if (lo >= hi) {
        return;
    }

    for (i = 0; i < CHUNK_NEIGHBOUR_COUNT; i++) {
        ChunkSlot* slot = &self->slots[i];
        GteLong* o;
        if (slot->loader->headerReady == 0 || slot->loader->chunkIndex < 0) {
            continue;
        }
        o = slot->cellParent->coord2->coord.t;
        for (k = 0; k < STAGE_SLOT_LATTICE_CELLS; k++) {
            GridCell* cell = slot->cells[k];
            s32 x = o[0] + (k % STAGE_CHUNK_CELLS) * STAGE_CELL_SIZE;
            s32 z = o[2] + (k / STAGE_CHUNK_CELLS) * STAGE_CELL_SIZE;
            s32 ahead = aheadX ? x : z;
            float dx, dz;
            if (!(cell->attribute & GsDOFF) || (cell->model == NULL && cell->nextInCell == NULL)) {
                continue; // shown by the game already, or empty
            }
            if (ahead < lo || ahead >= hi) {
                continue;
            }
            dx = (float)(x + STAGE_CELL_SIZE / 2 - pos[0]);
            dz = (float)(z + STAGE_CELL_SIZE / 2 - pos[2]);
            if (!InCone(dx, dz, fx, fz, slope, margin) && !InCone(dx, dz, px, pz, slope, margin)) {
                continue;
            }
            SetCellShown(cell, 1);
            sExtras[sExtraCount].slot = i;
            sExtras[sExtraCount].chunk = slot->loader->chunkIndex;
            sExtras[sExtraCount].cell = k;
            sExtraCount++;
        }
    }
}

static void WideRefreshFootprint(StageMap* self) {
    int same = self == sExtraMap;
    HideExtras(self);
    sRefreshFootprint(self);
    if (sWide && self->chunksLoaded != 0 && self->config->isVertical == 0) {
        if (!same) {
            sPrevYaw = self->target->coord2->param->rotate.vy;
        }
        ShowExtras(self);
        sPrevYaw = self->target->coord2->param->rotate.vy;
    }
}

static void (*sVariantSpriteReset)(VariantSprite* self, s32 variant);
static void (*sVariantSpriteUpdateScale)(VariantSprite* self, s32 set, Ratio16* ratios);

static void UnplainSprite(VariantSprite* self) {
    if (self->sprite.scalex == ONE && self->sprite.scaley == ONE) {
        self->sprite.scalex = ONE + 1;
    }
}

static void WideVariantSpriteReset(VariantSprite* self, s32 variant) {
    sVariantSpriteReset(self, variant);
    UnplainSprite(self);
}

static void WideVariantSpriteUpdateScale(VariantSprite* self, s32 set, Ratio16* ratios) {
    sVariantSpriteUpdateScale(self, set, ratios);
    UnplainSprite(self);
}

static void WideDayTaskOnInit(DayTask* self, s32 a, s32 b, s32 c) {
    sDayTaskOnInit(self, a, b, c);
    EnterWide();
}

static void WideDayTaskOnDeinit(DayTask* self) {
    LeaveWide();
    sDayTaskOnDeinit(self);
}

int Widescreen_Parse(const char* aspect, float* ratio) {
    unsigned w, h;
    char end;
    if (sscanf(aspect, "%u:%u%c", &w, &h, &end) != 2 || w == 0 || h == 0) {
        return -1;
    }
    *ratio = (float)w / (float)h;
    return 0;
}

void Widescreen_Init(float ratio) {
    const float console = 4.0f / 3.0f;
    if (ratio <= console + 0.001f) {
        return; // 4:3, or narrower: the console's picture
    }
    sStretch = ratio / console;
    sScaleX = (int)(0x10000 / sStretch + 0.5f);
    Psyz_VideoSetWindowAspect(ratio);
    sDayTaskOnInit = gDayTaskMethods.onInit;
    sDayTaskOnDeinit = gDayTaskMethods.onDeinit;
    gDayTaskMethods.onInit = WideDayTaskOnInit;
    gDayTaskMethods.onDeinit = WideDayTaskOnDeinit;
    sRefreshFootprint = gStageMapMethods.refreshFootprint;
    gStageMapMethods.refreshFootprint = WideRefreshFootprint;
    sVariantSpriteReset = (void*)gVariantSpriteMethods.reset;
    sVariantSpriteUpdateScale = (void*)gVariantSpriteMethods.updateScale;
    gVariantSpriteMethods.reset = (void*)WideVariantSpriteReset;
    gVariantSpriteMethods.updateScale = (void*)WideVariantSpriteUpdateScale;
}
