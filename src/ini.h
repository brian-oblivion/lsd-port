#ifndef LSD_INI_H
#define LSD_INI_H

// The port's .ini files: lines of `name = value`, `#` starting a comment,
// blank lines ignored.

// Called once per setting line, name and value trimmed; value may be
// changed in place. Returns 0, or -1 after saying on stderr what is wrong
// with the line (path:lineNo), which is then skipped.
typedef int (*IniApplyFn)(void* ctx, const char* name, char* value, const char* path, int lineNo);

// Reads path and calls apply for each setting. Returns 0, or -1 if the file
// cannot be read (missing, usually).
int Ini_Read(const char* path, IniApplyFn apply, void* ctx);

// Strips spaces, tabs and a CR from both ends of s, in place.
char* Ini_Trim(char* s);

#endif
