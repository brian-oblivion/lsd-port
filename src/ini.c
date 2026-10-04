#include "ini.h"

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
