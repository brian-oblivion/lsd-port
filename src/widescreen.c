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
// the window was shifted away from, as near as 5 cells (10000 units) ahead.
// src/far_map.c shows the cells in the wider view cone within the window's
// rows (or columns) ahead, from the game's chunks and the ones past them it
// loads, by Widescreen_Stretch.
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
// The ratio is taken up when a dream starts, so a change from the settings
// menu (Widescreen_Set) shows from the next one; at 4:3 the wrappers below
// leave everything as the game has it.
//
// Built with the game's C (it needs DayTask's and VariantSprite's method
// tables), not with the port's other files.

#include "common.h"
#include "day_task.h"
#include "variant_sprite.h"
#include "widescreen.h"

#include <psyz/gte.h>
#include <psyz/video.h>
#include <stdio.h>

static int sScaleX = 0x10000; // the GTE's X scale in the dream, 16.16
static float sStretch = 1.0f; // the display's stretch in the dream
// The same for the next dream (Widescreen_Set).
static int sNextScaleX = 0x10000;
static float sNextStretch = 1.0f;
static int sWide;             // 1 while a DayTask is initialised

// onInit's slot type: IntermediateBase__Init calls it as onInit(self, 0, 0, 0).
static void (*sDayTaskOnInit)(DayTask* self, s32 a, s32 b, s32 c);
static void (*sDayTaskOnDeinit)(DayTask* self);

static void EnterWide(void) {
    sScaleX = sNextScaleX;
    sStretch = sNextStretch;
    if (sStretch <= 1.0f) {
        return; // 4:3: the console's picture
    }
    sWide = 1;
    Psyz_GteSetScreenXScale(sScaleX);
    Psyz_VideoSetDisplayStretch(sStretch);
}

static void LeaveWide(void) {
    sWide = 0;
    Psyz_GteSetScreenXScale(0x10000);
    Psyz_VideoSetDisplayStretch(1.0f);
}

static void (*sVariantSpriteReset)(VariantSprite* self, s32 variant);
static void (*sVariantSpriteUpdateScale)(VariantSprite* self, s32 set, Ratio16* ratios);

static void UnplainSprite(VariantSprite* self) {
    if (sNextStretch > 1.0f && self->sprite.scalex == ONE && self->sprite.scaley == ONE) {
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

float Widescreen_Stretch(void) {
    return sWide ? sStretch : 1.0f;
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

void Widescreen_Set(float ratio) {
    const float console = 4.0f / 3.0f;
    if (ratio <= console + 0.001f) {
        // 4:3, or narrower: the console's picture
        sNextStretch = 1.0f;
        sNextScaleX = 0x10000;
        return;
    }
    sNextStretch = ratio / console;
    sNextScaleX = (int)(0x10000 / sNextStretch + 0.5f);
}

void Widescreen_Init(float ratio) {
    Widescreen_Set(ratio);
    if (sNextStretch > 1.0f) {
        Psyz_VideoSetWindowAspect(ratio);
    }
    sDayTaskOnInit = gDayTaskMethods.onInit;
    sDayTaskOnDeinit = gDayTaskMethods.onDeinit;
    gDayTaskMethods.onInit = WideDayTaskOnInit;
    gDayTaskMethods.onDeinit = WideDayTaskOnDeinit;
    sVariantSpriteReset = (void*)gVariantSpriteMethods.reset;
    sVariantSpriteUpdateScale = (void*)gVariantSpriteMethods.updateScale;
    gVariantSpriteMethods.reset = (void*)WideVariantSpriteReset;
    gVariantSpriteMethods.updateScale = (void*)WideVariantSpriteUpdateScale;
}
