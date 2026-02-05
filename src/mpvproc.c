/* File: mpvproc.c
 * Date: 2026-02-04
 *
 * MPV process forking and communication.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define UDS_PATH "/tmp/mpv.sock"

/* Based on MPV JSON-based IPC protocol */
#define STR_PROP_EOF "\"event\":\"end-file\""
#define STR_PROP_EOF_REASON "\"reason\":\"eof\""
#define STR_PROP_SOF "\"event\":\"start-file\""

#define CMD_PAUSE "{ \"command\": " \
    "[\"cycle\", \"pause\"] }\n"
#define CMD_LOAD  "{ \"command\": " \
    "[\"loadfile\", \"%s\", \"replace\"] }\n"

struct ProcMPV {
    pid_t pid;
    int fd;
} pmpv;

static bool wait_for_socket(void)
{
    struct stat st;
    int timed_out = 0;
    while (stat(UDS_PATH, &st) == -1) {
        if (timed_out > 15) {
            /* Give up after 1.5 seconds */
            fprintf(stderr, "Failed to detect UDS.\n");
            return false;
        }
        usleep(100000);
        timed_out++;
    }

    return true;
}

static bool connect_to_mpv(void)
{
    pmpv.fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, UDS_PATH);

    int timed_out = 0;
    while (connect(pmpv.fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        // Sleep until MPV is listening on the socket
        if (timed_out > 15) {
            /* Give up after 1.5 seconds */
            fprintf(stderr, "Failed to connect to MPV\n");
            return false;
        }
        usleep(100000);
        timed_out++;
    }

    return true;
}

void mpv_terminate(void)
{
    kill(pmpv.pid, SIGTERM);
    close(pmpv.fd);
}

int mpv_init(void)
{
    pmpv.pid = fork();
    
    if (pmpv.pid == 0) {
        execl(
            "/usr/bin/mpv",
            "mpv",
            "--input-ipc-server=/tmp/mpv.sock",
            "--idle",
            "--no-terminal",
            (char *) NULL
        );
        fprintf(stderr, "Error, unable to start MPV\n");
        _exit(127); /* 127: Command not found in PATH */
    } else {
        if (!wait_for_socket()) {
            return -1;
        }
        if (!connect_to_mpv()) {
            return -1;
        }
    }

    return pmpv.fd;
}

void mpv_load_song(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_LOAD, path);
    write(pmpv.fd, buf, strlen(buf));
}

void mpv_cycle_pause(void)
{
    write(pmpv.fd, CMD_PAUSE, strlen(CMD_PAUSE));
}

typedef enum {
    PROP_NONE,
    PROP_EOF,
    PROP_SOF,
} MPVProp;

MPVProp mpv_property(int fd)
{
    char haystack[4096];
    ssize_t n = read(fd, haystack, sizeof(haystack)-1);
    if (n > 0) {
        haystack[n] = '\0';
        if (strstr(haystack, STR_PROP_EOF) &&
            strstr(haystack, STR_PROP_EOF_REASON)) {
            return PROP_EOF;
        }
        /* Implement for autoplay/playlists */
        //if (strstr(haystack, STR_PROP_SOF)) {
        //    return PROP_SOF;
        //}
    }
    return PROP_NONE;
}

