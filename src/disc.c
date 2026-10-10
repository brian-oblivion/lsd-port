// Disc_ReadFile: one file of the disc image, for the port's own use (the
// settings menu's font). The .cue's first FILE names the image, its first
// TRACK the sector layout: MODE2/2352 (the game's) keeps a sector's 2048
// bytes of data after 24 bytes of sync, header and subheader, MODE1/2352
// after 16; MODE1/2048 is the data alone. The file system is ISO 9660: the
// primary volume descriptor at sector 16 gives the root directory, and each
// directory is a run of records naming its files and directories.

#include "disc.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <string.h>

#define SECTOR_DATA 2048

typedef struct {
    SDL_IOStream* io;
    int sectorSize; // bytes a sector takes in the image
    int dataOffset; // where its 2048 bytes of data start
} Image;

static unsigned Le32(const unsigned char* p) {
    return p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static int ReadSector(const Image* img, unsigned lba, unsigned char* out) {
    Sint64 at = (Sint64)lba * img->sectorSize + img->dataOffset;
    return SDL_SeekIO(img->io, at, SDL_IO_SEEK_SET) == at &&
           SDL_ReadIO(img->io, out, SECTOR_DATA) == SECTOR_DATA;
}

// Whether the TRACK line at track says mode.
static int TrackIs(const char* track, const char* mode) {
    const char* eol = strchr(track, '\n');
    const char* at = strstr(track, mode);
    return at != NULL && (eol == NULL || at < eol);
}

// The image's path and layout from the .cue: its first FILE and TRACK.
static int OpenImage(const char* cuePath, Image* img) {
    size_t size;
    char* cue = SDL_LoadFile(cuePath, &size);
    char* path = NULL;
    if (cue == NULL) {
        return -1;
    }
    const char* file = strstr(cue, "FILE");
    const char* track = strstr(cue, "TRACK");
    const char* name = file != NULL ? strchr(file, '"') : NULL;
    const char* end = name != NULL ? strchr(name + 1, '"') : NULL;
    img->sectorSize = 0;
    if (track != NULL && TrackIs(track, "MODE2/2352")) {
        img->sectorSize = 2352;
        img->dataOffset = 24;
    } else if (track != NULL && TrackIs(track, "MODE1/2352")) {
        img->sectorSize = 2352;
        img->dataOffset = 16;
    } else if (track != NULL && TrackIs(track, "MODE1/2048")) {
        img->sectorSize = 2048;
        img->dataOffset = 0;
    }
    if (end != NULL && img->sectorSize != 0) {
        // The image beside the .cue, unless its path is absolute.
        int dirLen = 0;
        for (int i = 0; cuePath[i] != '\0'; i++) {
            if (cuePath[i] == '/' || cuePath[i] == '\\') {
                dirLen = i + 1;
            }
        }
        if (name[1] == '/' || name[1] == '\\' || (name[1] != '\0' && name[2] == ':')) {
            dirLen = 0;
        }
        SDL_asprintf(&path, "%.*s%.*s", dirLen, cuePath, (int)(end - name - 1), name + 1);
    }
    SDL_free(cue);
    if (path == NULL) {
        return -1;
    }
    img->io = SDL_IOFromFile(path, "rb");
    SDL_free(path);
    return img->io != NULL ? 0 : -1;
}

// The record named name (case aside, without its ";1") in the directory
// at lba, size bytes: its extent and size. Returns 0, or -1.
static int FindRecord(const Image* img, unsigned lba, unsigned size, const char* name,
                      size_t nameLen, unsigned* outLba, unsigned* outSize) {
    unsigned char sector[SECTOR_DATA];
    for (unsigned done = 0; done < size; done += SECTOR_DATA, lba++) {
        if (!ReadSector(img, lba, sector)) {
            return -1;
        }
        for (unsigned at = 0; at < SECTOR_DATA && sector[at] != 0; at += sector[at]) {
            const unsigned char* rec = sector + at;
            if (at + 33 + rec[32] > SECTOR_DATA) {
                break;
            }
            size_t len = rec[32];
            const char* id = (const char*)rec + 33;
            const char* semi = memchr(id, ';', len);
            if (semi != NULL) {
                len = (size_t)(semi - id);
            }
            if (len == nameLen && SDL_strncasecmp(id, name, len) == 0) {
                *outLba = Le32(rec + 2);
                *outSize = Le32(rec + 10);
                return 0;
            }
        }
    }
    return -1;
}

void* Disc_ReadFile(const char* cuePath, const char* path, size_t* size) {
    Image img;
    unsigned char sector[SECTOR_DATA];
    unsigned lba, len;
    unsigned char* data = NULL;

    if (cuePath == NULL || OpenImage(cuePath, &img) != 0) {
        return NULL;
    }
    if (!ReadSector(&img, 16, sector) || sector[0] != 1 || memcmp(sector + 1, "CD001", 5) != 0) {
        SDL_CloseIO(img.io);
        return NULL;
    }
    lba = Le32(sector + 156 + 2);
    len = Le32(sector + 156 + 10);
    for (const char* part = path; *part != '\0';) {
        const char* sep = strchr(part, '\\');
        size_t partLen = sep != NULL ? (size_t)(sep - part) : strlen(part);
        if (FindRecord(&img, lba, len, part, partLen, &lba, &len) != 0) {
            SDL_CloseIO(img.io);
            return NULL;
        }
        part += partLen + (sep != NULL);
    }

    data = SDL_malloc(len + 1);
    for (unsigned done = 0; data != NULL && done < len; done += SECTOR_DATA) {
        unsigned n = len - done < SECTOR_DATA ? len - done : SECTOR_DATA;
        if (!ReadSector(&img, lba + done / SECTOR_DATA, sector)) {
            SDL_free(data);
            data = NULL;
            break;
        }
        memcpy(data + done, sector, n);
    }
    SDL_CloseIO(img.io);
    if (data != NULL) {
        *size = len;
    }
    return data;
}
