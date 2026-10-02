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

// libgte

VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1) {
    NOT_IMPLEMENTED;
    return v1;
}
SVECTOR* ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1) {
    NOT_IMPLEMENTED;
    return v1;
}
MATRIX* MulMatrix2(MATRIX* m0, MATRIX* m1) {
    NOT_IMPLEMENTED;
    return m1;
}
VECTOR* Square0(VECTOR* v0, VECTOR* v1) {
    NOT_IMPLEMENTED;
    return v1;
}
u_long* RCpolyF3(void* s, DIVPOLYGON3* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyF4(void* s, DIVPOLYGON4* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyFT3(void* s, DIVPOLYGON3* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyFT4(void* s, DIVPOLYGON4* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyG3(void* s, DIVPOLYGON3* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyG4(void* s, DIVPOLYGON4* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyGT3(void* s, DIVPOLYGON3* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}
u_long* RCpolyGT4(void* s, DIVPOLYGON4* divp) {
    NOT_IMPLEMENTED;
    return NULL;
}

// libgs

void GsInit3D(void) { NOT_IMPLEMENTED; }
void GsMapModelingData(u_long* base) { NOT_IMPLEMENTED; }
void GsLinkObject4(u_long tmd_base, GsDOBJ2* objp, int n) { NOT_IMPLEMENTED; }
void GsInitCoordinate2(GsCOORDINATE2* super, GsCOORDINATE2* base) {
    NOT_IMPLEMENTED;
}
void GsGetLs(GsCOORDINATE2* coord, MATRIX* m) { NOT_IMPLEMENTED; }
void GsGetLws(GsCOORDINATE2* coord, MATRIX* lw, MATRIX* ls) {
    NOT_IMPLEMENTED;
}
void GsGetTimInfo(u_long* im, GsIMAGE* tim) { NOT_IMPLEMENTED; }
void GsSetAmbient(long r, long g, long b) { NOT_IMPLEMENTED; }
int GsSetFlatLight(int id, GsF_LIGHT* light) {
    NOT_IMPLEMENTED;
    return 0;
}
void GsSetLightMatrix(MATRIX* mp) { NOT_IMPLEMENTED; }
void GsSetLightMode(int mode) { NOT_IMPLEMENTED; }
void GsSetLsMatrix(MATRIX* mp) { NOT_IMPLEMENTED; }
void GsSetNearClip(long clip_near) { NOT_IMPLEMENTED; }
void GsSetProjection(long h) { NOT_IMPLEMENTED; }
int GsSetRefView2(GsRVIEW2* pv) {
    NOT_IMPLEMENTED;
    return 0;
}
void GsSortBg(GsBG* bg, GsOT* otp, unsigned short pri) { NOT_IMPLEMENTED; }
void GsSortBoxFill(GsBOXF* boxf, GsOT* otp, unsigned short pri) {
    NOT_IMPLEMENTED;
}
void GsSortSprite(GsSPRITE* sprite, GsOT* otp, unsigned short pri) {
    NOT_IMPLEMENTED;
}

// libsnd

void SsSeqPause(short seq_access_num) { NOT_IMPLEMENTED; }
void SsSeqReplay(short seq_access_num) { NOT_IMPLEMENTED; }
void SsSetMute(char mode) { NOT_IMPLEMENTED; }
short SsUtAutoVol(short vc, short start_vol, short end_vol, short delta_time) {
    NOT_IMPLEMENTED;
    return 0;
}
short SsUtGetVabHdr(short vabId, VabHdr* vabhdrptr) {
    NOT_IMPLEMENTED;
    return -1;
}

// libcd

void StClearRing(void) { NOT_IMPLEMENTED; }
void StUnSetRing(void) { NOT_IMPLEMENTED; }

// libapi

void SetMem(unsigned long n) { NOT_IMPLEMENTED; }
