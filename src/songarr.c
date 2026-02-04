/* File: songarr.c
 * Date: 2026-02-04
 *
 * SongArr ADT.
 */

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "songarr.h"

#define FILEARR_INIT_CAP 16

int compare_songnames(const void *p, const void *q)
{
    SFile *song1 = (SFile *)p;
    SFile *song2 = (SFile *)q;

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

static bool create_sfile(SFile *sf, const char *entry, const char *dirname)
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

static bool songarr_realloc_check(SongArr *songarr)
{
    if (songarr->size >= songarr->cap) {
        songarr->cap *= 2;
        SFile *tmp = realloc(songarr->arr, songarr->cap * sizeof(SFile));
        if (tmp == NULL) {
            return false;
        }
        songarr->arr = tmp;
    }
    return true;
}

static bool scan_dir(const char *dirname, SongArr *songarr)
{
    DIR *pdir;
    struct dirent *entry;
    bool exit_status = false;

    if ((pdir = opendir(dirname)) == NULL) {
        goto out;
    }

    while ((entry = readdir(pdir)) != NULL) {
        switch (entry->d_type) {
            case DT_DIR: {
                if (entry->d_name[0] == '.') {
                    break;
                }
                char *full_path = cat_path(dirname, entry->d_name);
                if (full_path == NULL) {
                    goto out;
                }
                if (!scan_dir(full_path, songarr)) {
                    free(full_path);
                    goto out;
                }
                free(full_path);
                break;
            }
            case DT_REG: {
                if (!songarr_realloc_check(songarr)) {
                    goto out;
                }
                SFile sf;
                if (!create_sfile(&sf, entry->d_name, dirname)) {
                    goto out;
                }
                songarr->arr[songarr->size++] = sf;
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

void songarr_destroy(SongArr *songarr)
{
    for (size_t i = 0; i < songarr->size; i++) {
        free(songarr->arr[i].name);
        free(songarr->arr[i].path);
    }
    free(songarr->arr);
    free(songarr);
}

SongArr *songarr_init(const char *dirname)
{
    SongArr *songarr = malloc(sizeof(SongArr));
    if (songarr == NULL) {
        return NULL;
    }
    songarr->arr = malloc(FILEARR_INIT_CAP * sizeof(SFile));
    if (songarr->arr == NULL) {
        free(songarr);
        return NULL;
    }

    songarr->cap = FILEARR_INIT_CAP;
    songarr->size = 0;
    if (!scan_dir(dirname, songarr)) {
        songarr_destroy(songarr);
        return NULL;
    }
    qsort(songarr->arr, songarr->size, sizeof(SFile), compare_songnames);

    return songarr;
}

