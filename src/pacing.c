// Pace and smooth: when the dream's ticks come, and what is drawn between
// them.
//
// The game paces itself: DrawSystem__RunLoop calls VSync(3), then its
// callback, then notifies its parents. On that notification the Viewport
// flips (draws the ordering table the last pass built) and the dream's
// FrameClock ticks, which has the Viewport build the next table (the world
// as the last tick left it) before the dream's logic runs. One tick every
// three vertical blanks: 20 a second.
//
// In a dream (while a DayTask is initialised: its onInit and onDeinit are
// wrapped here, as widescreen.c does) PacedRunLoop takes RunLoop's place,
// one pass per vertical blank of psyz's 59.94 Hz:
//  - every pass presents and waits one blank (Psyz_VideoVSync(0));
//  - every 60 / pace blanks the pass is a tick: what VSync(3) does once its
//    wait is over (Psyz_VSyncRunCallbacks: the pads and the debug server
//    once, the VSyncCallback ones, such as the CD driver's service, three
//    times), then RunLoop's callback and notification. A tick sees what it
//    sees at 20, only further apart. The music isn't the game's loop: psyz
//    runs libsnd's sequencer from the audio thread.
//  - with smooth on, the passes between ticks draw a frame each, as a tick's
//    pass does (the Viewport's flip, then its update), with every node that
//    moved between where the last tick drew it and where its logic has since
//    put it. The view hangs off DreamSys's coordinate, so that moves the
//    camera too. Every coordinate the draw can touch is put back afterwards,
//    so the game's state is what it was; only libgs's frame counter and the
//    buffers move on. A move too large for one tick (a link, a respawn) is
//    not blended, nor are a TodActor's parts and the grid cells (IsBlended).
// Menus, the post-day graph and the movies keep the game's own loop.
//
// Built with the game's C (it needs DayTask's and DrawSystem's method
// tables), not with the port's other files.

#include "common.h"
#include "day_task.h"
#include "draw_system.h"
#include "grid_cell.h"
#include "scene_node.h"
#include "viewport.h"
#include "pacing.h"

#include <libetc.h>
#include <psyz/system.h>
#include <psyz/video.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// A tick every 60 / pace blanks: `phase` counts blanks times the pace.
#define TICK_PHASE 60

// The most DreamSys moves in one tick and is still blended. Measured over four
// days of every tick: walking moves it up to about 130 along an axis, some
// stages carry it 256 or 512 a tick for a while, and a turn is 6 degrees (68
// in 4096ths of a turn); a link or a respawn moves it 9824 or more.
#define JUMP_DISTANCE 4096
#define JUMP_ANGLE 512 // 45 degrees

static int sPace = PACING_PACE_GAME;
static int sSmooth;

static void (*sDayTaskOnInit)(DayTask* self, s32 a, s32 b, s32 c);
static void (*sDayTaskOnDeinit)(DayTask* self);

// The dream being run; NULL outside one.
static Viewport* sViewport;
static SceneNode* sDreamSys;

// TodActor's class id (gTodActorMethods' header): the nodes below one are the
// parts its TOD animation moves.
#define TODACTOR_CLASS_ID 0x234

// What is blended: a node's offset from its parent and its rotation.
typedef struct {
    int t[3];
    SVECTOR rotate;
} Pose;

// A node as the last tick drew it.
typedef struct {
    SceneNode* node;
    GsCOORDINATE2* coord;
    Pose pose;
} Drawn;

static Drawn* sDrawn;
static int sDrawnCount;
static int sDrawnCap;
static int sHaveDrawn;
static Pose sCamera; // DreamSys's entry, which decides whether to blend at all

// sDrawn's indices by node, open addressing; -1 is empty.
static int* sIndex;
static unsigned sIndexMask;

// The coordinates and rotations an in-between draw can change, as they were.
typedef struct {
    GsCOORDINATE2* coord;
    GsCOORDINATE2 saved;
} SavedCoord;

typedef struct {
    GsCOORD2PARAM* param;
    SVECTOR rotate;
} SavedRotate;

static SavedCoord* sSaved;
static int sSavedCount;
static int sSavedCap;
static SavedRotate* sSavedRotates;
static int sSavedRotateCount;
static int sSavedRotateCap;

// Makes room for one more element in a growing array.
static void* Grow(void* array, int count, int* cap, size_t size) {
    if (count < *cap) {
        return array;
    }
    *cap = *cap ? *cap * 2 : 256;
    array = realloc(array, size * (size_t)*cap);
    if (array == NULL) {
        abort();
    }
    return array;
}

static void GetPose(SceneNode* node, Pose* pose) {
    GsCOORDINATE2* coord = node->coord2;
    for (int i = 0; i < 3; i++) {
        pose->t[i] = coord->coord.t[i];
    }
    pose->rotate = coord->param->rotate;
}

static int SamePose(const Pose* a, const Pose* b) {
    return a->t[0] == b->t[0] && a->t[1] == b->t[1] && a->t[2] == b->t[2] &&
           a->rotate.vx == b->rotate.vx && a->rotate.vy == b->rotate.vy &&
           a->rotate.vz == b->rotate.vz;
}

// The shortest way from angle a to b, in 4096ths of a turn.
static int AngleDelta(int a, int b) {
    return ((b - a + 2048) & 4095) - 2048;
}

static int IsJump(const Pose* a, const Pose* b) {
    for (int i = 0; i < 3; i++) {
        if (abs(b->t[i] - a->t[i]) > JUMP_DISTANCE) {
            return 1;
        }
    }
    return abs(AngleDelta(a->rotate.vx, b->rotate.vx)) > JUMP_ANGLE ||
           abs(AngleDelta(a->rotate.vy, b->rotate.vy)) > JUMP_ANGLE ||
           abs(AngleDelta(a->rotate.vz, b->rotate.vz)) > JUMP_ANGLE;
}

static int Blend(int a, int b, int i, int n) {
    return a + (int)((long long)(b - a) * i / n);
}

static short BlendAngle(int a, int b, int i, int n) {
    return (short)(a + AngleDelta(a, b) * i / n);
}

static unsigned Hash(SceneNode* node) {
    unsigned long long h = (unsigned long long)(uintptr_t)node * 0x9E3779B97F4A7C15ull;
    return (unsigned)(h >> 32);
}

static Drawn* FindDrawn(SceneNode* node) {
    if (sIndex == NULL) {
        return NULL;
    }
    for (unsigned h = Hash(node) & sIndexMask;; h = (h + 1) & sIndexMask) {
        if (sIndex[h] < 0) {
            return NULL;
        }
        if (sDrawn[sIndex[h]].node == node) {
            return &sDrawn[sIndex[h]];
        }
    }
}

static void IndexDrawn(void) {
    unsigned size = 256;
    while (size < (unsigned)sDrawnCount * 2) {
        size *= 2;
    }
    if (size - 1 != sIndexMask || sIndex == NULL) {
        free(sIndex);
        sIndex = malloc(sizeof(*sIndex) * size);
        if (sIndex == NULL) {
            abort();
        }
        sIndexMask = size - 1;
    }
    memset(sIndex, 0xFF, sizeof(*sIndex) * size);
    for (int i = 0; i < sDrawnCount; i++) {
        unsigned h = Hash(sDrawn[i].node) & sIndexMask;
        while (sIndex[h] >= 0) {
            h = (h + 1) & sIndexMask;
        }
        sIndex[h] = i;
    }
}

static int IsSceneNodeChild(BasicClass* child, SceneNode* parent) {
    return child != NULL && (child->methods->header & CLASS_ID_ROOT_MASK) == SCENENODE_CLASS_ID &&
           ((SceneNode*)child)->parent == parent;
}

static int IsTodActor(SceneNode* node) {
    return (node->methods->header & CLASS_ID_LEVEL3_MASK) == TODACTOR_CLASS_ID;
}

// Whether a node's moves are blended: not a TodActor's part, whose moves are
// its animation, and not a GridCell, which the StageMap moves by whole cells
// (2048) as the player walks, to reuse it on the other side.
static int IsBlended(SceneNode* node, int isPart) {
    return !isPart && (u8)node->methods->header != GRIDCELL_CLASS_ID && node->coord2 != NULL &&
           node->coord2->param != NULL;
}

// Records the pose of `node` and the SceneNodes below it, as
// Viewport__DrawNode walks them, of those IsBlended.
static void RecordTree(SceneNode* node, int isPart) {
    BasicClassListNode* cursor = node->children;
    BasicClass* child;

    if (IsBlended(node, isPart)) {
        sDrawn = Grow(sDrawn, sDrawnCount, &sDrawnCap, sizeof(*sDrawn));
        Drawn* d = &sDrawn[sDrawnCount++];
        d->node = node;
        d->coord = node->coord2;
        GetPose(node, &d->pose);
    }
    isPart = isPart || IsTodActor(node);
    while (cursor != NULL) {
        GetNextBasicClass(&child, &cursor);
        if (IsSceneNodeChild(child, node)) {
            RecordTree((SceneNode*)child, isPart);
        }
    }
}

// Saves every coordinate of `node` and the SceneNodes below it, and puts each
// recorded node that has moved since i / n of the way from its recorded pose
// to its current one (or at its recorded pose, after a jump).
static void BlendTree(SceneNode* node, int isPart, int i, int n) {
    BasicClassListNode* cursor = node->children;
    BasicClass* child;
    GsCOORDINATE2* coord = node->coord2;
    Drawn* d;
    Pose now;

    if (coord != NULL) {
        sSaved = Grow(sSaved, sSavedCount, &sSavedCap, sizeof(*sSaved));
        sSaved[sSavedCount].coord = coord;
        sSaved[sSavedCount].saved = *coord;
        sSavedCount++;
    }
    if (IsBlended(node, isPart) && (d = FindDrawn(node)) != NULL && d->coord == coord) {
        GetPose(node, &now);
        if (!SamePose(&d->pose, &now)) {
            int k = IsJump(&d->pose, &now) ? 0 : i;
            sSavedRotates = Grow(sSavedRotates, sSavedRotateCount, &sSavedRotateCap,
                                 sizeof(*sSavedRotates));
            sSavedRotates[sSavedRotateCount].param = coord->param;
            sSavedRotates[sSavedRotateCount].rotate = coord->param->rotate;
            sSavedRotateCount++;
            for (int a = 0; a < 3; a++) {
                coord->coord.t[a] = Blend(d->pose.t[a], now.t[a], k, n);
            }
            coord->param->rotate.vx = BlendAngle(d->pose.rotate.vx, now.rotate.vx, k, n);
            coord->param->rotate.vy = BlendAngle(d->pose.rotate.vy, now.rotate.vy, k, n);
            coord->param->rotate.vz = BlendAngle(d->pose.rotate.vz, now.rotate.vz, k, n);
            coord->flg = 0;
        }
    }
    isPart = isPart || IsTodActor(node);
    while (cursor != NULL) {
        GetNextBasicClass(&child, &cursor);
        if (IsSceneNodeChild(child, node)) {
            BlendTree((SceneNode*)child, isPart, i, n);
        }
    }
}

static void Restore(void) {
    for (int i = sSavedRotateCount - 1; i >= 0; i--) {
        sSavedRotates[i].param->rotate = sSavedRotates[i].rotate;
    }
    for (int i = sSavedCount - 1; i >= 0; i--) {
        *sSaved[i].coord = sSaved[i].saved;
    }
    sSavedRotateCount = 0;
    sSavedCount = 0;
}

// Before a tick: what its update is about to draw.
static void RecordDrawn(void) {
    Viewport* vp = sViewport;

    sHaveDrawn = 0;
    if (vp->otReady == 0 || vp->viewNode != sDreamSys || sDreamSys->coord2->param == NULL) {
        return;
    }
    sDrawnCount = 0;
    RecordTree(GetRootNode(vp->viewNode), 0);
    RecordTree(vp->sceneRoot, 0);
    IndexDrawn();
    GetPose(sDreamSys, &sCamera);
    sHaveDrawn = 1;
}

// The frame i / n of the way from the last tick's picture to the next one's.
static void DrawInBetween(int i, int n) {
    Viewport* vp = sViewport;
    Pose camera;

    if (!sHaveDrawn || vp->otReady == 0 || vp->viewNode != sDreamSys) {
        return;
    }
    GetPose(sDreamSys, &camera);
    if (IsJump(&sCamera, &camera)) {
        return; // the tick's own picture until the next
    }

    vp->methods->flip(vp);
    BlendTree(GetRootNode(vp->viewNode), 0, i, n);
    BlendTree(vp->sceneRoot, 0, i, n);
    vp->methods->update(vp);
    Restore();
}

// DrawSystem__RunLoop with the dream paced here.
static void PacedRunLoop(DrawSystem* self) {
    int phase = 0;
    int pass = 0;     // passes since the last tick
    int interval = 1; // passes from the last tick to the next

    while (self->running != 0) {
        if (sViewport == NULL) {
            VSync(self->vsyncCount);
            phase = 0;
            pass = 0;
            interval = 1;
        } else {
            Psyz_VideoVSync(0);
            phase += sPace;
            if (phase < TICK_PHASE) {
                // Spread evenly over the passes until the next tick, which
                // at a pace that doesn't divide 60 are 4 or 5 (at 14):
                // blending by the phase instead makes a tick that comes a
                // blank late stand out.
                if (sSmooth && ++pass < interval) {
                    DrawInBetween(pass, interval);
                }
                continue;
            }
            phase -= TICK_PHASE;
            pass = 0;
            interval = (TICK_PHASE - phase + sPace - 1) / sPace;
            Psyz_VSyncRunCallbacks(self->vsyncCount);
        }
        // This pass's update draws the world as it is now; the logic after
        // it moves it on.
        if (sSmooth && sViewport != NULL) {
            RecordDrawn();
        }
        if (self->callback != NULL) {
            self->callback();
        }
        self->methods->notifyParents(self, DRAWSYSTEM_EVENT_VSYNC);
    }
}

static void PacedDayTaskOnInit(DayTask* self, s32 a, s32 b, s32 c) {
    sDayTaskOnInit(self, a, b, c);
    sViewport = (Viewport*)self->viewport;
    sDreamSys = (SceneNode*)self->dreamSys;
    sHaveDrawn = 0;
}

static void PacedDayTaskOnDeinit(DayTask* self) {
    sViewport = NULL;
    sDreamSys = NULL;
    sHaveDrawn = 0;
    sDayTaskOnDeinit(self);
}

void Pacing_Init(int pace, int smooth) {
    if (pace == PACING_PACE_GAME && !smooth) {
        return; // the game's own pacing
    }
    sPace = pace;
    sSmooth = smooth;
    gDrawSystemMethods.runLoop = PacedRunLoop;
    sDayTaskOnInit = gDayTaskMethods.onInit;
    sDayTaskOnDeinit = gDayTaskMethods.onDeinit;
    gDayTaskMethods.onInit = PacedDayTaskOnInit;
    gDayTaskMethods.onDeinit = PacedDayTaskOnDeinit;
}
