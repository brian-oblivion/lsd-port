// Draw distance: the dream's fog moved further away, so more of a stage
// shows before it fades into the sky.
//
// Each stage's style picks a fog distance (fogNear, 26624 down to 4096), and
// ObjM hands it to the dream's Viewport through setFogNear; the Viewport's
// update gives it to SetFogNear, whose depth cue darkens a face from that
// distance on and has the TMD renderer drop it at five times it. Wrapping
// setFogNear here multiplies the distance by the setting, so the fade and
// the cull move out together.
//
// The farthest it goes is the game's clearest fog, 26624 (its stages with
// fog level 0): SetFogNear's depth-cue slope, -320 * fogNear / h, must fit
// the GTE's 16-bit DQA, which at the dream's projection (h = 266) allows
// 27238. With that fog nothing is culled within the GTE's depth range, and
// what ends the view is the map's footprint, the 20 cells ahead of the
// player that the StageMap draws, as on those stages.
//
// The settings menu changes it while a dream runs (DrawDistance_Set): the
// distance the game last gave the dream's Viewport is kept, and the
// Viewport, which is the DayTask's, is given it again at the new scale; its
// update hands it on from the next frame. At 1 the wrapper passes the game's
// distance through unchanged.
//
// Built with the game's C (it needs the Viewport's and DayTask's method
// tables), not with the port's other files.

#include "common.h"
#include "day_task.h"
#include "node_guarded_viewport.h"
#include "draw_distance.h"

// sStyleFogNears[0], the clearest fog a stage has.
#define FOG_NEAR_MAX 26624

static int sScale = 1;

static void (*sSetFogNear)(NodeGuardedViewport* self, s32 fogNear);
static void (*sDayTaskOnDeinit)(DayTask* self);

// The Viewport the game last gave a distance, and that distance; NULL
// outside a dream.
static NodeGuardedViewport* sFogViewport;
static s32 sGameFogNear;

static s32 Further(s32 fogNear) {
    s32 further = fogNear * sScale;
    if (further > FOG_NEAR_MAX) {
        further = fogNear > FOG_NEAR_MAX ? fogNear : FOG_NEAR_MAX;
    }
    return further;
}

static void FarSetFogNear(NodeGuardedViewport* self, s32 fogNear) {
    sFogViewport = self;
    sGameFogNear = fogNear;
    sSetFogNear(self, Further(fogNear));
}

static void FarDayTaskOnDeinit(DayTask* self) {
    sFogViewport = NULL;
    sDayTaskOnDeinit(self);
}

void DrawDistance_Set(int scale) {
    sScale = scale;
    if (sFogViewport != NULL) {
        sSetFogNear(sFogViewport, Further(sGameFogNear));
    }
}

void DrawDistance_Init(int scale) {
    sScale = scale;
    sSetFogNear = gNodeGuardedViewportMethods.setFogNear;
    gNodeGuardedViewportMethods.setFogNear = FarSetFogNear;
    sDayTaskOnDeinit = gDayTaskMethods.onDeinit;
    gDayTaskMethods.onDeinit = FarDayTaskOnDeinit;
}
