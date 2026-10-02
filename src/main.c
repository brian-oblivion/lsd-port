// Placeholder entry point: brings psyz up and draws a line of text, so the
// build, the platform layer and CI can be exercised before any game C is
// compiled. The game's own main (decomp/src) replaces this.

#include <libetc.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <psyz/dbgserver.h>
#include <psyz/video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 1

static GsOT sOt[2];
static GsOT_TAG sOtTags[2][1 << OT_LENGTH];
static PACKET sPackets[2][64];

int main(int argc, char** argv) {
    // --frames N: exit after N frames (for smoke tests); 0 runs forever.
    int frames = 0;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--frames") == 0) {
            frames = atoi(argv[i + 1]);
        }
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

    SetVideoMode(MODE_NTSC);
    ResetGraph(0);
    GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsNONINTER | GsOFSGPU, 1, 0);
    GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);
    for (int i = 0; i < 2; i++) {
        sOt[i].length = OT_LENGTH;
        sOt[i].org = sOtTags[i];
        GsClearOt(0, 0, &sOt[i]);
    }

    FntLoad(960, 256);
    SetDumpFnt(FntOpen(16, 16, SCREEN_WIDTH - 32, SCREEN_HEIGHT - 32, 0, 512));

    for (int frame = 0; frames == 0 || frame < frames; frame++) {
        FntPrint("lsd-port skeleton\n\nframe %d\n", frame);
        FntFlush(-1);
        int buf = GsGetActiveBuff();
        GsSetWorkBase(sPackets[buf]);
        GsClearOt(0, 0, &sOt[buf]);
        DrawSync(0);
        VSync(0);
        GsSwapDispBuff();
        GsSortClear(0, 0, 0, &sOt[buf]);
        GsDrawOt(&sOt[buf]);
    }
    return 0;
}
