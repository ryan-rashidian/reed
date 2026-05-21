/* 
 * File: songarr.c
 *
 * Song playlist array.
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "songarr.h"

#define FILE_ARR_START_CAP 32

int compare_songnames(const void *p, const void *q)
{
    SongFile *song1 = (SongFile *)p;
    SongFile *song2 = (SongFile *)q;

    if (strcmp(song1->name, song2->name) < 0) {
        return -1;
    } else if (strcmp(song1->name, song2->name) > 0) {
        return 1;
    } else {
        return 0;
    }
}

static char *cat_path(const char *root, const char *branch)
{
    size_t size = strlen(root) + strlen(branch) + 2;
    char *full_path = malloc(size);
    if (full_path == NULL) {
        return NULL;
    }

    strcpy(full_path, root);
    strcat(full_path, "/");
    strcat(full_path, branch);

    return full_path;
}

static bool create_sfile(SongFile *sf, const char *entry, const char *dirname)
{
    sf->name = malloc(strlen(entry)+1);
    if (sf->name == NULL) {
        return false;
    }

    strcpy(sf->name, entry);
    sf->path = cat_path(dirname, entry);
    if (sf->path == NULL) {
        free(sf->name);
        return false;
    }

    return true;
}

static bool songarr_realloc_check(SongArr *song_arr)
{
    if (song_arr->size >= song_arr->cap) {
        song_arr->cap *= 2;
        size_t new_size = song_arr->cap * sizeof(SongFile);
        SongFile *tmp = realloc(song_arr->arr, new_size);
        if (tmp == NULL) {
            return false;
        }
        song_arr->arr = tmp;
    }
    return true;
}

static bool scan_dir(const char *dir_name, SongArr *song_arr)
{
    DIR *pdir;
    struct dirent *entry;
    bool exit_status = false;

    if ((pdir = opendir(dir_name)) == NULL) {
        goto out;
    }

    while ((entry = readdir(pdir)) != NULL) {
        switch (entry->d_type) {
            case DT_DIR: {
                if (entry->d_name[0] == '.') {
                    break;
                }
                char *full_path = cat_path(dir_name, entry->d_name);
                if (full_path == NULL) {
                    goto out;
                }
                if (!scan_dir(full_path, song_arr)) {
                    free(full_path);
                    goto out;
                }
                free(full_path);
                break;
            }
            case DT_REG: {
                if (!songarr_realloc_check(song_arr)) {
                    goto out;
                }
                SongFile sf;
                if (!create_sfile(&sf, entry->d_name, dir_name)) {
                    goto out;
                }
                song_arr->arr[song_arr->size++] = sf;
                break;
            }
            default: break;
        }
    }
    exit_status = true;

    out:
    if (pdir != NULL) {
        closedir(pdir);
    }
    return exit_status;
}

void song_arr_destroy(SongArr *song_arr)
{
    for (size_t i = 0; i < song_arr->size; i++) {
        free(song_arr->arr[i].name);
        free(song_arr->arr[i].path);
    }
    free(song_arr->arr);
    free(song_arr);
}

SongArr *song_arr_init(const char *dir_name)
{
    SongArr *song_arr = malloc(sizeof(SongArr));
    if (song_arr == NULL) {
        return NULL;
    }
    song_arr->arr = malloc(FILE_ARR_START_CAP * sizeof(SongFile));
    if (song_arr->arr == NULL) {
        free(song_arr);
        return NULL;
    }

    song_arr->cap = FILE_ARR_START_CAP;
    song_arr->size = 0;
    if (!scan_dir(dir_name, song_arr)) {
        song_arr_destroy(song_arr);
        return NULL;
    }
    qsort(song_arr->arr, song_arr->size, sizeof(SongFile), compare_songnames);

    return song_arr;
}

