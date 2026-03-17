#pragma once

/*
 * Cross-platform file enumeration (glob replacement)
 * Provides glob-like functionality on Windows and Unix systems
 */

#ifdef _WIN32
#include <windows.h>
#else
#include <glob.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cross-platform glob structure
typedef struct {
    size_t gl_pathc;        // Number of matched paths
    char **gl_pathv;        // Array of matched paths
    size_t gl_offs;         // Offset into gl_pathv
    
#ifdef _WIN32
    // Windows-specific data
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    char pattern[MAX_PATH];
    char directory[MAX_PATH];
    int finished;
#else
    // Unix uses standard glob_t
    glob_t unix_glob;
#endif
} xglob_t;

// Flags for xglob
#define XGLOB_ERR       0x01    // Return on read errors
#define XGLOB_MARK      0x02    // Mark directories with trailing slash
#define XGLOB_NOSORT    0x04    // Don't sort filenames
#define XGLOB_DOOFFS    0x08    // Use gl_offs
#define XGLOB_NOCHECK   0x10    // Return pattern if no match
#define XGLOB_APPEND    0x20    // Append to previous results
#define XGLOB_NOESCAPE  0x40    // Don't handle backslashes specially

// Error return values
#define XGLOB_NOSPACE   1       // Memory allocation failure
#define XGLOB_ABORTED   2       // Read error
#define XGLOB_NOMATCH   3       // No matches found

// Function prototypes
int xglob(const char *pattern, int flags, int (*errfunc)(const char *, int), xglob_t *pglob);
void xglobfree(xglob_t *pglob);

// Helper functions
int xglob_match_pattern(const char *pattern, const char *string);
void xglob_normalize_path(char *path);

#ifdef __cplusplus
}
#endif