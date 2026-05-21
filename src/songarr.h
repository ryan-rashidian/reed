/* 
 * File: songarr.c
 *
 * Song playlist array.
 */

#ifndef SONGARR_H
#define SONGARR_H

#include <stdlib.h>

typedef struct {
    char *name;
    char *path;
} SongFile;

typedef struct {
    size_t size;
    size_t cap;
    SongFile *arr;
} SongArr;

SongArr *song_arr_init(const char *dir_name);
void song_arr_destroy(SongArr *song_arr);

#endif

