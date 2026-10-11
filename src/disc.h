#ifndef LSD_DISC_H
#define LSD_DISC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reads a file from the disc image's ISO 9660 file system by its path on
// the disc ("ETC\\FONTICON.TIM"), from the first data track of the .cue's
// image, through a file handle of its own: psyz's libcd and its position
// in the image are left alone. Returns the bytes (SDL_free them) and their
// count in *size, or NULL when the image or the file can't be read.
void* Disc_ReadFile(const char* cuePath, const char* path, size_t* size);

// The disc image held open, for reads by sector (logical block address,
// from 0 at the start of the data track; a CdlLOC's minus 150): the same
// image and handle of its own as Disc_ReadFile. NULL when it can't be
// opened.
typedef struct Disc Disc;
Disc* Disc_Open(const char* cuePath);
// size bytes from sector lba on into out. Returns 0, or -1.
int Disc_Read(Disc* disc, unsigned lba, void* out, size_t size);
void Disc_Close(Disc* disc);

#ifdef __cplusplus
}
#endif

#endif
