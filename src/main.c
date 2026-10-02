// The host's entry point: reads the port's own options, brings up the tools
// psyz offers, then runs the game's main (decomp/src/main.c, compiled with
// main renamed to lsd_game_main), which never returns.
//
// Exit codes: 2 when there is no usable disc image; otherwise the game's
// (0 after --frames N).

#include <psyz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lsd_game_main(void);

// --frames N: exit after N VSyncs, for smoke tests; 0 runs forever.
static int sFrameLimit;
static int sFrameCount;
static PsyzVSyncCb sNextVSyncCb;

static void CountFrame(void) {
    if (sNextVSyncCb != NULL) {
        sNextVSyncCb();
    }
    if (sFrameLimit > 0 && ++sFrameCount >= sFrameLimit) {
        exit(0);
    }
}

int main(int argc, char** argv) {
    // --disc FILE.cue (or LSD_DISC): the user's disc image, which psyz's
    // libcd reads. The game needs it from its first file on.
    const char* disc = getenv("LSD_DISC");
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--frames") == 0) {
            sFrameLimit = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "--disc") == 0) {
            disc = argv[i + 1];
        }
    }
    if (disc == NULL) {
        fprintf(stderr, "lsd: no disc image; pass --disc path/to/game.cue\n");
        return 2;
    }
    if (Psyz_CdSetDiskPath(disc) != 0) {
        fprintf(stderr, "lsd: cannot read the disc image %s\n", disc);
        return 2;
    }
    if (sFrameLimit > 0) {
        sNextVSyncCb = Psyz_SetVSyncCb(CountFrame);
    }

    // LSD_DEBUG_PORT=<port>: psyz's debug server on 127.0.0.1 (screenshots,
    // VRAM, input injection) for tools and agents.
    const char* debugPort = getenv("LSD_DEBUG_PORT");
    if (debugPort != NULL) {
        int port = Psyz_DebugServer(atoi(debugPort));
        fprintf(stderr, "debug server on 127.0.0.1:%d\n", port);
    }

    // LSD_VSYNC=auto|on|off|limitless: psyz's frame pacing. "off" paces with
    // psyz's own limiter (59.94 fps), so the game keeps running while its
    // window is hidden or when there is no display.
    static const struct {
        const char* name;
        PsyzVsyncMode mode;
    } vsyncModes[] = {
        {"auto", PSYZ_VSYNC_AUTO},
        {"on", PSYZ_VSYNC_ON},
        {"off", PSYZ_VSYNC_OFF},
        {"limitless", PSYZ_VSYNC_LIMITLESS},
    };
    const char* vsync = getenv("LSD_VSYNC");
    for (size_t i = 0; vsync != NULL && i < sizeof(vsyncModes) / sizeof(*vsyncModes); i++) {
        if (strcmp(vsync, vsyncModes[i].name) == 0) {
            Psyz_VideoSetVsyncMode(vsyncModes[i].mode);
        }
    }

    lsd_game_main();
    return 0;
}
