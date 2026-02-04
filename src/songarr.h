/* File: songarr.h
 * Date: 2026-02-04
 *
 * SongArr ADT.
 */

#ifndef SONGARR_H
#define SONGARR_H

#include <stdlib.h>

typedef struct {
    char *name;
    char *path;
} SFile;

typedef struct {
    size_t size;
    size_t cap;
    SFile *arr;
} SongArr;

SongArr *songarr_init(const char *dirname);
void songarr_destroy(SongArr *songarr);

#endif

