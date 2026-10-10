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

#ifdef __cplusplus
}
#endif

#endif
