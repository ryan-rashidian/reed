/* 
 * songarr - Song playlist array.
 *
 * Uses dirent.h (and sys/stat.h as a fallback) to search a directory, and all
 * of its child directories for files. Currently does not distinguish between
 * file types, and presumes that all file paths can be passed to MPV.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>

#include "songarr.h"

#define FILE_ARR_START_CAP 32

// Forward function declarations
static void scan_directory(SongArr *song_arr, const char *dir_name);

static int cmp_songs(const void *p, const void *q)
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
    char *path = malloc(size);
    if (path == NULL) return NULL;

    strcpy(path, root);
    strcat(path, "/");
    strcat(path, branch);

    return path;
}

static bool create_song_file(
    SongFile *sf,
    const char *entry_name,
    const char *dir_name
)
{
    sf->name = malloc(strlen(entry_name)+1);
    if (sf->name == NULL) {
        return false;
    }

    strcpy(sf->name, entry_name);
    sf->path = cat_path(dir_name, entry_name);
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
        if (tmp == NULL) return false;
        song_arr->arr = tmp;
    }
    return true;
}

static void handle_reg(
    SongArr *song_arr,
    const char *dir_name,
    char *entry_name
)
{
    SongFile sf;

    if (!songarr_realloc_check(song_arr)) return;
    if (!create_song_file(&sf, entry_name, dir_name)) return;

    song_arr->arr[song_arr->size++] = sf;
}

static void handle_dir(
    SongArr *song_arr,
    const char *dir_name,
    char *entry_name
)
{
    char *full_path = cat_path(dir_name, entry_name);
    if (full_path == NULL) return;

    scan_directory(song_arr, full_path);
    free(full_path);
}

static void handle_unknown(
    SongArr *song_arr,
    const char *dir_name,
    char *entry_name
)
{
    // Fallback to using sys/stat if dirent can't identify the entry
    struct stat st;
    if (stat(entry_name, &st) == 0) {
        if        (S_ISDIR(st.st_mode)) {
            handle_dir(song_arr, dir_name, entry_name);
        } else if (S_ISREG(st.st_mode)) {
            handle_reg(song_arr, dir_name, entry_name);
        }
    }
    
}

static void identify_entry(
    SongArr *song_arr,
    const char *dir_name,
    struct dirent *entry
)
{
    switch (entry->d_type) {
        case DT_REG: {
            handle_reg(song_arr, dir_name, entry->d_name);
        } break;
        case DT_DIR: {
            handle_dir(song_arr, dir_name, entry->d_name);
        } break;
        case DT_UNKNOWN: {
            handle_unknown(song_arr, dir_name, entry->d_name);
        } break;
        default: break;
    }
}

static void scan_directory(SongArr *song_arr, const char *dir_name)
{
    DIR *dirp;
    struct dirent *entry;

    if ((dirp = opendir(dir_name)) == NULL) {
        fprintf(stderr, "dirp: Failed to open file: %s\n", dir_name);
        return;
    }

    while ((entry = readdir(dirp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        identify_entry(song_arr, dir_name, entry);
    }

    closedir(dirp);
}

// Public interface
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
    if (song_arr == NULL) return NULL;

    song_arr->arr = malloc(FILE_ARR_START_CAP * sizeof(SongFile));
    if (song_arr->arr == NULL) {
        free(song_arr);
        return NULL;
    }

    song_arr->cap = FILE_ARR_START_CAP;
    song_arr->size = 0;
    scan_directory(song_arr, dir_name);
    qsort(song_arr->arr, song_arr->size, sizeof(SongFile), cmp_songs);

    return song_arr;
}

