#include "ini.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <stdio.h>
#include <string.h>

char* Ini_Trim(char* s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        *--end = '\0';
    }
    return s;
}

int Ini_Read(const char* path, IniApplyFn apply, void* ctx) {
    size_t size;
    char* text = SDL_LoadFile(path, &size);
    if (text == NULL) {
        return -1;
    }
    int lineNo = 0;
    char* next = NULL;
    for (char* line = text; line != NULL; line = next) {
        next = strchr(line, '\n');
        if (next != NULL) {
            *next++ = '\0';
        }
        lineNo++;
        char* hash = strchr(line, '#');
        if (hash != NULL) {
            *hash = '\0';
        }
        line = Ini_Trim(line);
        if (*line == '\0') {
            continue;
        }
        char* eq = strchr(line, '=');
        if (eq == NULL) {
            fprintf(stderr, "lsd: %s:%d: expected name = value\n", path, lineNo);
            continue;
        }
        *eq = '\0';
        apply(ctx, Ini_Trim(line), Ini_Trim(eq + 1), path, lineNo);
    }
    SDL_free(text);
    return 0;
}

// The setting a line sets: its name, trimmed, into name (at most size - 1
// characters); 0 when the line sets nothing. With commented, only a line
// that sets one behind a `#` ("# name = value") counts instead.
static int LineSetting(const char* line, int commented, char* name, size_t size) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (commented) {
        if (*line != '#') {
            return 0;
        }
        line++;
        while (*line == ' ' || *line == '\t') {
            line++;
        }
    }
    const char* eq = strchr(line, '=');
    const char* hash = strchr(line, '#');
    if (eq == NULL || (hash != NULL && hash < eq) || eq == line) {
        return 0;
    }
    const char* end = eq;
    while (end > line && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    if ((size_t)(end - line) >= size) {
        return 0;
    }
    for (const char* c = line; c < end; c++) {
        if (*c == ' ' || *c == '\t') {
            return 0; // a sentence in a comment, not a setting
        }
    }
    SDL_memcpy(name, line, (size_t)(end - line));
    name[end - line] = '\0';
    return 1;
}

// The setting in set named name, or -1.
static int FindSetting(const IniSetting* set, int count, const char* name) {
    for (int i = 0; i < count; i++) {
        if (SDL_strcasecmp(set[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int Ini_Update(const char* path, const IniSetting* set, int count) {
    size_t size = 0;
    char* text = SDL_LoadFile(path, &size);
    const char* eol = text != NULL && strstr(text, "\r\n") != NULL ? "\r\n" : "\n";
    int lineCount = 1;
    for (size_t i = 0; i < size; i++) {
        lineCount += text[i] == '\n';
    }
    char** lines = SDL_calloc((size_t)lineCount, sizeof(*lines));
    char* written = SDL_calloc((size_t)count + 1, 1);
    int n = 0;
    char* next = NULL;
    for (char* line = text; line != NULL && n < lineCount; line = next) {
        next = strchr(line, '\n');
        if (next != NULL) {
            *next++ = '\0';
        } else if (*line == '\0') {
            break; // the newline at the end of the last line
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        lines[n++] = line;
    }

    // Lines that set one: the value replaced, a comment after it kept.
    char** made = SDL_calloc((size_t)n + 1, sizeof(*made));
    char name[64];
    for (int i = 0; i < n; i++) {
        int s;
        if (!LineSetting(lines[i], 0, name, sizeof(name)) ||
            (s = FindSetting(set, count, name)) < 0) {
            continue;
        }
        const char* hash = strchr(lines[i], '#');
        SDL_asprintf(&made[i], "%s = %s%s%s", set[s].name, set[s].value, hash ? "  " : "",
                     hash ? hash : "");
        written[s] = 1;
    }
    // Those still to write: a commented-out line for them, or the end.
    for (int i = 0; i < n; i++) {
        int s;
        if (made[i] != NULL || !LineSetting(lines[i], 1, name, sizeof(name)) ||
            (s = FindSetting(set, count, name)) < 0 || written[s]) {
            continue;
        }
        SDL_asprintf(&made[i], "%s = %s", set[s].name, set[s].value);
        written[s] = 1;
    }

    char* tmp;
    SDL_asprintf(&tmp, "%s.new", path);
    SDL_IOStream* io = SDL_IOFromFile(tmp, "wb");
    int result = -1;
    if (io != NULL) {
        for (int i = 0; i < n; i++) {
            SDL_IOprintf(io, "%s%s", made[i] != NULL ? made[i] : lines[i], eol);
        }
        for (int s = 0; s < count; s++) {
            if (!written[s]) {
                SDL_IOprintf(io, "%s = %s%s", set[s].name, set[s].value, eol);
            }
        }
        if (SDL_CloseIO(io) && SDL_RenamePath(tmp, path)) {
            result = 0;
        }
    }
    if (result != 0) {
        fprintf(stderr, "lsd: cannot write %s: %s\n", path, SDL_GetError());
        SDL_RemovePath(tmp);
    }
    for (int i = 0; i < n; i++) {
        SDL_free(made[i]);
    }
    SDL_free(made);
    SDL_free(tmp);
    SDL_free(written);
    SDL_free(lines);
    SDL_free(text);
    return result;
}
