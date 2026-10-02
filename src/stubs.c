// Temporary stand-ins, so that the game links before the platform layer is
// complete. Every one is a to-do item, not a fix; see
// docs/research/host-link-surface.md.
//
// SDK functions psyz does not have yet: each logs once, the way psyz's own
// NOT_IMPLEMENTED stubs do, and returns zero.

#include <psyz.h>
#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>
#include <libsnd.h>
#include <libcd.h>
#include <libapi.h>

// libsnd

void SsSeqPause(short seq_access_num) { NOT_IMPLEMENTED; }
void SsSeqReplay(short seq_access_num) { NOT_IMPLEMENTED; }
void SsSetMute(char mode) { NOT_IMPLEMENTED; }
short SsUtAutoVol(short vc, short start_vol, short end_vol, short delta_time) {
    NOT_IMPLEMENTED;
    return 0;
}

// libcd

// libapi

void SetMem(unsigned long n) { NOT_IMPLEMENTED; }
