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
// Built with the game's C (it needs DayTask's method table), not with the
// port's other files.

#include "common.h"
#include "day_task.h"
#include "widescreen.h"

#include <psyz/gte.h>
#include <psyz/video.h>
#include <stdio.h>

static int sScaleX = 0x10000; // the GTE's X scale in the dream, 16.16
static float sStretch = 1.0f; // the display's stretch in the dream

// onInit's slot type: IntermediateBase__Init calls it as onInit(self, 0, 0, 0).
static void (*sDayTaskOnInit)(DayTask* self, s32 a, s32 b, s32 c);
static void (*sDayTaskOnDeinit)(DayTask* self);

static void EnterWide(void) {
    Psyz_GteSetScreenXScale(sScaleX);
    Psyz_VideoSetDisplayStretch(sStretch);
}

static void LeaveWide(void) {
    Psyz_GteSetScreenXScale(0x10000);
    Psyz_VideoSetDisplayStretch(1.0f);
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
}
