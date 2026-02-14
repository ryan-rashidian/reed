/* File: mpvproc.h
 * Date: 2026-02-04
 *
 * MPV process forking and communication.
 */

#ifndef MPVPROC_H
#define MPVPROC_H

typedef enum {
    PROP_NONE,
    PROP_EOF,
} MPVProp;

int mpv_init(void);
void mpv_terminate(void);
void mpv_load_song(const char *path);
void mpv_cycle_pause(void);
void mpv_seek(int time);
void mpv_volume(int vol);
MPVProp mpv_property(int fd);

#endif

