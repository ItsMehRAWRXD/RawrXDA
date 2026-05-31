// mmap_file.h
// Cross-platform memory-mapped file loader for RawrXD
// Supports Windows (MapViewOfFile) and POSIX (mmap)

#ifndef MMAP_FILE_H
#define MMAP_FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include ?cntl.h>
    #include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *addr;
    size_t size;
    #if defined(_WIN32)
    HANDLE h_file;
    HANDLE h_map;
    #else
    int fd;
    #endif
} MMapFile;

// Open and memory-map a file for reading
static inline MMapFile* mmap_file_open(const char *path) {
    MMapFile *mf = (MMapFile*)calloc(1, sizeof(MMapFile));
    if (!mf) return NULL;

    #if defined(_WIN32)
    mf->h_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mf->h_file == INVALID_HANDLE_VALUE) {
        free(mf);
        return NULL;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(mf->h_file, &size)) {
        CloseHandle(mf->h_file);
        free(mf);
        return NULL;
    }
    mf->size = (size_t)size.QuadPart;

    mf->h_map = CreateFileMapping(mf->h_file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mf->h_map) {
        CloseHandle(mf->h_file);
        free(mf);
        return NULL;
    }

    mf->addr = MapViewOfFile(mf->h_map, FILE_MAP_READ, 0, 0, mf->size);
    if (!mf->addr) {
        CloseHandle(mf->h_map);
        CloseHandle(mf->h_file);
        free(mf);
        return NULL;
    }
    #else
    mf->fd = open(path, O_RDONLY);
    if (mf->fd < 0) {
        free(mf);
        return NULL;
    }

    struct stat st;
    if (fstat(mf->fd, &st) < 0) {
        close(mf->fd);
        free(mf);
        return NULL;
    }
    mf->size = st.st_size;

    mf->addr = mmap(NULL, mf->size, PROT_READ, MAP_PRIVATE, mf->fd, 0);
    if (mf->addr == MAP_FAILED) {
        close(mf->fd);
        free(mf);
        return NULL;
    }
    #endif

    return mf;
}

// Close and unmap a file
static inline void mmap_file_close(MMapFile *mf) {
    if (!mf) return;

    #if defined(_WIN32)
    if (mf->addr) UnmapViewOfFile(mf->addr);
    if (mf->h_map) CloseHandle(mf->h_map);
    if (mf->h_file != INVALID_HANDLE_VALUE) CloseHandle(mf->h_file);
    #else
    if (mf->addr != MAP_FAILED) munmap(mf->addr, mf->size);
    if (mf->fd >= 0) close(mf->fd);
    #endif

    free(mf);
}

// Get pointer to file data
static inline void* mmap_file_data(MMapFile *mf) {
    return mf ? mf->addr : NULL;
}

// Get file size
static inline size_t mmap_file_size(MMapFile *mf) {
    return mf ? mf->size : 0;
}

#ifdef __cplusplus
}
#endif

#endif // MMAP_FILE_H
