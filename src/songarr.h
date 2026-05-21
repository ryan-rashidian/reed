/* 
 * songarr - Song playlist array.
 *
 * Uses dirent.h (and sys/stat.h as a fallback) to search a directory, and all
 * of its child directories for files. Currently does not distinguish between
 * file types, and presumes that all file paths can be passed to MPV.
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

