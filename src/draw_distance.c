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
// Built with the game's C (it needs the Viewport's method table), not with
// the port's other files.

#include "common.h"
#include "node_guarded_viewport.h"
#include "draw_distance.h"

// sStyleFogNears[0], the clearest fog a stage has.
#define FOG_NEAR_MAX 26624

static int sScale = 1;

static void (*sSetFogNear)(NodeGuardedViewport* self, s32 fogNear);

static void FarSetFogNear(NodeGuardedViewport* self, s32 fogNear) {
    s32 further = fogNear * sScale;
    if (further > FOG_NEAR_MAX) {
        further = fogNear > FOG_NEAR_MAX ? fogNear : FOG_NEAR_MAX;
    }
    sSetFogNear(self, further);
}

void DrawDistance_Init(int scale) {
    if (scale <= 1) {
        return; // the console's fog
    }
    sScale = scale;
    sSetFogNear = gNodeGuardedViewportMethods.setFogNear;
    gNodeGuardedViewportMethods.setFogNear = FarSetFogNear;
}
