/*
 * Cross-platform file enumeration implementation
 * Replaces glob() with Windows-compatible implementation
 */

#include "headers/xglob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#endif

// Add a path to the result list
static int xglob_add_path(xglob_t *pglob, const char *path)
{
    char **new_pathv;
    size_t new_size = (pglob->gl_pathc + 1) * sizeof(char *);
    
    new_pathv = (char **)realloc(pglob->gl_pathv, new_size);
    if (!new_pathv)
        return XGLOB_NOSPACE;
    
    pglob->gl_pathv = new_pathv;
    pglob->gl_pathv[pglob->gl_pathc] = (char *)malloc(strlen(path) + 1);
    if (!pglob->gl_pathv[pglob->gl_pathc])
        return XGLOB_NOSPACE;
    
    strcpy(pglob->gl_pathv[pglob->gl_pathc], path);
    pglob->gl_pathc++;
    
    return 0;
}

#ifdef _WIN32

// Simple pattern matching for Windows
int xglob_match_pattern(const char *pattern, const char *string)
{
    const char *p = pattern;
    const char *s = string;
    
    while (*p && *s) {
        if (*p == '*') {
            // Skip consecutive asterisks
            while (*p == '*') p++;
            
            // If pattern ends with *, it matches rest of string
            if (!*p) return 1;
            
            // Find next matching character
            while (*s && *s != *p) s++;
            if (!*s) return 0;
            
        } else if (*p == '?') {
            // ? matches any single character
            p++;
            s++;
        } else if (*p == *s) {
            // Characters match
            p++;
            s++;
        } else {
            // No match
            return 0;
        }
    }
    
    // Handle trailing asterisks in pattern
    while (*p == '*') p++;
    
    // Both should be at end for complete match
    return (*p == 0 && *s == 0);
}

void xglob_normalize_path(char *path)
{
    char *p;
    
    // Convert forward slashes to backslashes
    for (p = path; *p; p++) {
        if (*p == '/')
            *p = '\\';
    }
}

int xglob(const char *pattern, int flags, int (*errfunc)(const char *, int), xglob_t *pglob)
{
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char search_pattern[MAX_PATH];
    char directory[MAX_PATH];
    char filename[MAX_PATH];
    char full_path[MAX_PATH];
    char *last_slash;
    
    // Initialize the glob structure
    memset(pglob, 0, sizeof(xglob_t));
    
    // Copy and normalize the pattern
    strncpy(search_pattern, pattern, sizeof(search_pattern) - 1);
    search_pattern[sizeof(search_pattern) - 1] = '\0';
    xglob_normalize_path(search_pattern);
    
    // Extract directory and filename pattern
    strcpy(directory, search_pattern);
    last_slash = strrchr(directory, '\\');
    if (last_slash) {
        *last_slash = '\0';
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(directory, ".");
        strcpy(filename, search_pattern);
    }
    
    // Build the Windows search pattern
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", directory);
    
    // Start the file search
    hFind = FindFirstFileA(search_pattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        if (flags & XGLOB_NOCHECK) {
            // Return the pattern itself if no matches and NOCHECK is set
            return xglob_add_path(pglob, pattern);
        }
        return XGLOB_NOMATCH;
    }
    
    do {
        // Skip "." and ".." directories
        if (strcmp(findData.cFileName, ".") == 0 || 
            strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        // Check if filename matches our pattern
        if (xglob_match_pattern(filename, findData.cFileName)) {
            // Build full path
            if (strcmp(directory, ".") == 0) {
                strcpy(full_path, findData.cFileName);
            } else {
                snprintf(full_path, sizeof(full_path), "%s\\%s", 
                        directory, findData.cFileName);
            }
            
            // Add trailing slash for directories if MARK flag is set
            if ((flags & XGLOB_MARK) && 
                (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                strcat(full_path, "\\");
            }
            
            // Add to results
            if (xglob_add_path(pglob, full_path) != 0) {
                FindClose(hFind);
                xglobfree(pglob);
                return XGLOB_NOSPACE;
            }
        }
        
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    
    // Check if we found anything
    if (pglob->gl_pathc == 0) {
        if (flags & XGLOB_NOCHECK) {
            return xglob_add_path(pglob, pattern);
        }
        return XGLOB_NOMATCH;
    }
    
    // Sort results if requested (simple bubble sort)
    if (!(flags & XGLOB_NOSORT) && pglob->gl_pathc > 1) {
        size_t i, j;
        for (i = 0; i < pglob->gl_pathc - 1; i++) {
            for (j = 0; j < pglob->gl_pathc - i - 1; j++) {
                if (strcmp(pglob->gl_pathv[j], pglob->gl_pathv[j + 1]) > 0) {
                    char *temp = pglob->gl_pathv[j];
                    pglob->gl_pathv[j] = pglob->gl_pathv[j + 1];
                    pglob->gl_pathv[j + 1] = temp;
                }
            }
        }
    }
    
    return 0;
}

#else

// Unix implementation - just wrap the standard glob()
int xglob(const char *pattern, int flags, int (*errfunc)(const char *, int), xglob_t *pglob)
{
    int unix_flags = 0;
    int result;
    
    // Convert flags
    if (flags & XGLOB_ERR) unix_flags |= GLOB_ERR;
    if (flags & XGLOB_MARK) unix_flags |= GLOB_MARK;
    if (flags & XGLOB_NOSORT) unix_flags |= GLOB_NOSORT;
    if (flags & XGLOB_DOOFFS) unix_flags |= GLOB_DOOFFS;
    if (flags & XGLOB_NOCHECK) unix_flags |= GLOB_NOCHECK;
    if (flags & XGLOB_APPEND) unix_flags |= GLOB_APPEND;
    if (flags & XGLOB_NOESCAPE) unix_flags |= GLOB_NOESCAPE;
    
    result = glob(pattern, unix_flags, errfunc, &pglob->unix_glob);
    
    // Copy results to our structure
    pglob->gl_pathc = pglob->unix_glob.gl_pathc;
    pglob->gl_pathv = pglob->unix_glob.gl_pathv;
    pglob->gl_offs = pglob->unix_glob.gl_offs;
    
    // Convert return values
    switch (result) {
        case 0: return 0;
        case GLOB_NOSPACE: return XGLOB_NOSPACE;
        case GLOB_ABORTED: return XGLOB_ABORTED;
        case GLOB_NOMATCH: return XGLOB_NOMATCH;
        default: return result;
    }
}

int xglob_match_pattern(const char *pattern, const char *string)
{
    return fnmatch(pattern, string, 0) == 0;
}

void xglob_normalize_path(char *path)
{
    // Unix paths are already normalized
    (void)path;
}

#endif

void xglobfree(xglob_t *pglob)
{
    if (!pglob)
        return;

#ifdef _WIN32
    size_t i;
    
    // Free each path string
    for (i = 0; i < pglob->gl_pathc; i++) {
        if (pglob->gl_pathv[i]) {
            free(pglob->gl_pathv[i]);
        }
    }
    
    // Free the array
    if (pglob->gl_pathv) {
        free(pglob->gl_pathv);
    }
    
    // Clear the structure
    memset(pglob, 0, sizeof(xglob_t));
#else
    globfree(&pglob->unix_glob);
    memset(pglob, 0, sizeof(xglob_t));
#endif
}