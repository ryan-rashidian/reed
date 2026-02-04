/* File: reed.c
 * Date: 2026-02-04
 *
 * TUI implementation with ncurses.
 */

#include <ncurses.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpvproc.h"
#include "songarr.h"

#define TITLE_MENU "> Songs <"
#define SUBTITLE_MENU "> ('q' - quit) reed 0.1.0 <"
#define TITLE_VIEW "> Playing <"
#define MAX_SONGTITLE_LEN 512

bool running;
SongArr *songarr;

struct pollfd fds[2];

struct PlayerState {
    bool playing;
    bool paused;
    char current_track[MAX_SONGTITLE_LEN+1];
} player = { .paused = false, .current_track[0] = '\0' };

typedef struct {
    int y, x;
} RowCol;

typedef struct {
    WINDOW *w;
    RowCol max;
    int offset_idx;
} Menu;

typedef struct {
    WINDOW *w;
    RowCol max;
} View;

struct UI {
    Menu menu;
    View view;
    RowCol max;
    RowCol curs;
} ui = { .curs = {1, 2}, .menu.offset_idx = 0 };

void program_destroy(void);

void ui_init_core(void)
{
    (void) initscr();
    (void) cbreak();
    (void) noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
}

void ui_init_windows(void)
{
    int y, x;
    getmaxyx(stdscr, y, x);

    ui.menu.w = newwin(y, x/2, 0, 0);
    ui.view.w = newwin(y, x/2, 0, (x/2)+(x%2));
    keypad(ui.menu.w, TRUE);
    keypad(ui.view.w, TRUE);
    nodelay(ui.menu.w, TRUE);
    nodelay(ui.view.w, TRUE);
}

void clear_window(WINDOW *w)
{
    /* Clear window, but keep the border. */
    werase(w);
    box(w, 0, 0);
}

void resize_windows(void)
{
    int y = ui.max.y;
    int x = ui.max.x;

    wresize(ui.menu.w, y, x/2);
    mvwin(  ui.menu.w, 0, 0);
    wresize(ui.view.w, y, x/2);
    mvwin(  ui.view.w, 0, (x/2)+(x%2));
}

void refresh_windows(void)
{
    /* More efficient than wrefresh on each window */
    wnoutrefresh(ui.menu.w);
    wnoutrefresh(ui.view.w);
    doupdate();
}

void update_maxyx(void)
{
    getmaxyx(stdscr, ui.max.y, ui.max.x);
    getmaxyx(ui.menu.w, ui.menu.max.y, ui.menu.max.x);
    getmaxyx(ui.view.w, ui.view.max.y, ui.view.max.x);
}

void draw_menu(void)
{
    int y = ui.menu.max.y;
    int x = ui.menu.max.x;
    int offset;
    int title_len = strlen(TITLE_MENU);
    offset = (title_len/2) + (title_len%2);
    int title_ctr_x = x/2 - offset;
    int subtitle_len = strlen(SUBTITLE_MENU);
    offset = (subtitle_len/2) + (subtitle_len%2);
    int subtitle_ctr_x = x/2 - offset;

    mvwprintw(ui.menu.w, 0, title_ctr_x, "%s", TITLE_MENU);
    mvwprintw(ui.menu.w, y-1, subtitle_ctr_x, "%s", SUBTITLE_MENU);

    /* Draw menu items */
    int max_cols = x - 5; /* -2 for border, -3 for " > " */
    int max_rows = y - 2; /* -2 for border */
    int j = ui.menu.offset_idx;
    for (int row = 0; row < max_rows && j < (int)songarr->size; row++, j++) {
        const char *name = songarr->arr[j].name;
        int str_len = strlen(name);
        if (str_len > max_cols) {
            char short_name[max_cols+1];
            snprintf(short_name, max_cols+1, "%s", name);
            mvwprintw(ui.menu.w, row+1, 1, " > %s", short_name);
        } else {
            mvwprintw(ui.menu.w, row+1, 1, " > %s", name);
        }
    }
}

void draw_viewer(void)
{
    int y = ui.view.max.y;
    int x = ui.view.max.x;
    int offset;

    int title_len = strlen(TITLE_VIEW);
    offset = (title_len/2) + (title_len%2);
    int title_ctr_x = x/2 - offset;
    mvwprintw(ui.view.w, 0, title_ctr_x, "%s", TITLE_VIEW);

    if (player.playing) {
        int max_cols = x - 2; /* -2 for border */
        const char *name = player.current_track;
        int track_len = strlen(name);
        if (track_len > max_cols) {
            char short_name[max_cols+1];
            snprintf(short_name, max_cols+1, "%s", name);
            mvwprintw(ui.view.w, y/2, 1, "%s", short_name);
        } else {
            offset = (track_len/2) + (track_len%2);
            int track_ctr_x = x/2 - offset;
            mvwprintw(ui.view.w, y/2, track_ctr_x, "%s", name);
        }
    }

    if (player.paused) {
        int pctr_x = x/2 - 5; /* midpoint - strlen("> PAUSE <") / 2 */
        mvwprintw(ui.view.w, y-1, pctr_x, "> PAUSE <");
    }
}

void resize_items(void)
{
    if (ui.menu.offset_idx != 0) {
        int max_rows = ui.max.y - 2; /* -2 for border */
        if (max_rows >= (int)songarr->size) {
            /* Reset offset index if window is large enough */
            ui.menu.offset_idx = 0;
        } else {
            /* Show more items if window is large enough */
            int diff = (int)songarr->size - max_rows;
            if (diff < ui.menu.offset_idx) {
                ui.menu.offset_idx = diff;
            }
        }
    }
}

void item_scroll_down(void)
{
    ui.menu.offset_idx++;
    clear_window(ui.menu.w);
    draw_menu();
}

void item_scroll_up(void)
{
    ui.menu.offset_idx--;
    clear_window(ui.menu.w);
    draw_menu();
}

void item_scroll_bottom(void)
{
    int max_rows = ui.max.y - 2; /* -2 for border */
    int diff = (int)songarr->size - max_rows;
    ui.menu.offset_idx = diff;
    clear_window(ui.menu.w);
    draw_menu();
}

void item_scroll_top(void)
{
    ui.menu.offset_idx = 0;
    clear_window(ui.menu.w);
    draw_menu();
}

void cursor_scroll_down(void)
{
    int max_rows = ui.max.y - 2; /* -2 for border */
    int items = (int)songarr->size;
    int off_scr = items - max_rows;

    if (ui.curs.y >= max_rows) {
        ui.curs.y = max_rows;
        if (off_scr > ui.menu.offset_idx) {
            item_scroll_down();
        }
        return;
    }
    if (ui.curs.y >= items) {
        ui.curs.y = items;
        return;
    }
    ui.curs.y++;
}

void cursor_scroll_up(void)
{
    if (ui.curs.y <= 1) {
        ui.curs.y = 1;
        if (ui.menu.offset_idx > 0) {
            item_scroll_up();
        }
        return;
    }
    ui.curs.y--;
}

void cursor_scroll_bottom(void)
{
    int max_rows = ui.max.y - 2; /* -2 for border */
    if (max_rows > (int)songarr->size) {
        ui.curs.y = songarr->size;
    } else {
        ui.curs.y = max_rows;
        item_scroll_bottom();
    }
}

void cursor_scroll_top(void)
{
    ui.curs.y = 1;
    if (ui.menu.offset_idx > 0) {
        item_scroll_top();
    }
}

void cursor_move_pos(void)
{
    int max_rows = ui.max.y - 2; /* -2 for border */
    if (ui.curs.y > max_rows) {
        ui.curs.y = max_rows;
    }
    if (ui.curs.y < 1) {
        ui.curs.y = 1;
    }
    wmove(ui.menu.w, ui.curs.y, ui.curs.x);
}

void switch_keypress(int key)
{
    switch (key) {
        case KEY_RESIZE: {
            update_maxyx();
            resize_windows();
            resize_items();
            clear_window(ui.menu.w);
            clear_window(ui.view.w);
            draw_menu();
            draw_viewer();
            refresh_windows();
            break;
        }
        case 'k':
        case KEY_UP: {
            cursor_scroll_up();
            break;
        }
        case 'G': {
            cursor_scroll_top();
            break;
        }
        case 'j':
        case KEY_DOWN: {
            cursor_scroll_down();
            break;
        }
        case 'g': {
            cursor_scroll_bottom();
            break;
        }
        case '\n':
        case KEY_ENTER: {
            int idx = ui.menu.offset_idx + ui.curs.y - 1;
            mpv_load_song(songarr->arr[idx].path);
            player.playing = true;
            strcpy(player.current_track, songarr->arr[idx].name);
            clear_window(ui.view.w);
            draw_viewer();
            wrefresh(ui.view.w);
            break;
        }
        case ' ':
        case 'p': {
            mpv_cycle_pause();
            player.paused = !player.paused;
            clear_window(ui.view.w);
            draw_viewer();
            wrefresh(ui.view.w);
            break;
        }
        case 'q': {
            running = false;
            break;
        }
        default: break;
    }
}

void handle_mpv_properties(void)
{
    MPVProp p = mpv_property(fds[0].fd);
    if (p == PROP_SOF) {
        /* Placeholder; Implement for autoplay/playlists */
        ;
    } else if (p == PROP_EOF) {
        /* End of song reached */
        player.playing = false;
        clear_window(ui.view.w);
        draw_viewer();
        wrefresh(ui.view.w);
    }
}

void ui_event_loop(void)
{
    /* Draw initial screen */
    update_maxyx();
    box(ui.menu.w, 0, 0);
    box(ui.view.w, 0, 0);
    draw_menu();
    draw_viewer();
    refresh_windows();

    /* Enter event loop */
    int ch;
    running = true;
    while (running) {
        cursor_move_pos();
        wrefresh(ui.menu.w);
        poll(fds, 2, -1); /* Blocking */
        if (fds[0].revents & POLLIN) {
            handle_mpv_properties();
        } else {
            ch = wgetch(ui.menu.w); /* Non-Blocking */
            if (ch != ERR) {
                switch_keypress(ch);
            }
        }
    }
}

void ui_destroy(void)
{
    delwin(ui.menu.w);
    delwin(ui.view.w);
    endwin();
}

void program_destroy(void)
{
    ui_destroy();
    mpv_terminate();
    songarr_destroy(songarr);
}

void handle_sigint(int sig)
{
    (void)sig;
    program_destroy();
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <music-dirname>\n", argv[0]);
        return 1;
    }
    (void)signal(SIGINT, handle_sigint);

    /* Search dir and build song playlist */
    songarr = songarr_init(argv[1]);
    if (songarr == NULL) {
        fprintf(stderr, "Error reading from directory: %s\n", argv[1]);
        return 1;
    }

    /* Setup polls for reading MPV properties */
    int mpv_fd = mpv_init();
    if (mpv_fd == -1) {
        fprintf(stderr, "Error initializing MPV\n");
        songarr_destroy(songarr);
        return 1;
    }
    fds[0].fd = mpv_fd;
    fds[0].events = POLLIN;
    fds[1].fd = 0;
    fds[1].events = POLLIN;

    /* Initialize UI and enter event loop */
    ui_init_core();
    ui_init_windows();
    ui_event_loop();

    /* Cleanup */
    program_destroy();
    return 0;
}

