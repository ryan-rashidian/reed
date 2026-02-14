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
#include <time.h>
#include <unistd.h>

#include "mpvproc.h"
#include "songarr.h"

#define TITLE_MENU "> Songs <"
#define SUBTITLE_MENU "> ('q' - quit) reed 0.3.0 <"
#define TITLE_VIEW "> Playing <"
#define MAX_SONGTITLE_LEN 512

#define LOOP_RUN 1
#define LOOP_STOP 0

bool songarr_initialized = false;
bool player_initialized = false;
bool mpv_initialized = false;
bool ncurses_initialized = false;

volatile sig_atomic_t running = LOOP_RUN;
SongArr *songarr;
struct pollfd fds[2];

struct PlayerState {
    bool playing;
    bool paused;
    bool autoplay;
    bool shuffle;
    int *order;
    int shuffle_idx;
    int curr_idx;
    char curr_track[MAX_SONGTITLE_LEN+1];
} player = {
    .paused = false,
    .autoplay = false,
    .shuffle = false,
    .curr_track[0] = '\0'
};

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

bool player_init(size_t n_songs)
{
    player.order = malloc(n_songs * sizeof(int));
    if (player.order == NULL) {
        fprintf(stderr, "Failed to allocate player.order array\n");
        return false;
    }

    for (int i = 0; i < (int)n_songs; i++) {
        player.order[i] = i;
    }
    return true;
}

bool ui_init_core(void)
{
    if (initscr() == NULL) {
        fprintf(stderr, "initscr failed to create main window\n");
        return false;
    }
    if (cbreak() == ERR) {
        fprintf(stderr, "cbreak failed to disable line buffering\n");
        endwin();
        return false;
    }
    (void) noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    return true;
}

bool ui_init_windows(void)
{
    int y, x;
    getmaxyx(stdscr, y, x);

    ui.menu.w = newwin(y, x/2, 0, 0);
    if (ui.menu.w == NULL) {
        fprintf(stderr, "newwin failed to create menu window\n");
        return false;
    }
    ui.view.w = newwin(y, x/2, 0, (x/2)+(x%2));
    if (ui.view.w == NULL) {
        fprintf(stderr, "newwin failed to create view window\n");
        delwin(ui.menu.w);
        return false;
    }
    keypad(ui.menu.w, TRUE);
    keypad(ui.view.w, TRUE);
    nodelay(ui.menu.w, TRUE);
    nodelay(ui.view.w, TRUE);
    return true;
}

void ui_destroy(void)
{
    delwin(ui.menu.w);
    delwin(ui.view.w);
    endwin();
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
        const char *name = player.curr_track;
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

    if (player.shuffle) {
        int ctr_x = x/2 - 5; /* Centering for "[Shuffle]" */
        mvwprintw(ui.view.w, y-2, ctr_x, "[Shuffle]");
    } else if (player.autoplay) {
        int ctr_x = x/2 - 6; /* Centering for "[Autoplay]" */
        mvwprintw(ui.view.w, y-2, ctr_x, "[Auto-Play]");
    }

    if (player.paused) {
        int ctr_x = x/2 - 5; /* Centering for "> PAUSE <" */
        mvwprintw(ui.view.w, y-1, ctr_x, "> PAUSE <");
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

void event_playsong(int idx) 
{
    if (idx >= (int)songarr->size) {
        player.playing = false;
        return;
    } else if (idx < 0) {
        idx = 0;
    }
    mpv_load_song(songarr->arr[idx].path);
    player.playing = true;
    player.curr_idx = idx;
    strncpy(
        player.curr_track,
        songarr->arr[idx].name,
        (size_t)MAX_SONGTITLE_LEN
    );
}

void event_shuffle(void)
{
    for (int i = (int)songarr->size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = player.order[i];
        player.order[i] = player.order[j];
        player.order[j] = tmp;
    }
    player.shuffle_idx = 0;
    player.shuffle = true;
}

int event_next(void)
{
    int idx;
    if (player.shuffle) {
        if (player.shuffle_idx + 1 >= (int)songarr->size) {
            return -1;
        }
        idx = player.order[++player.shuffle_idx];
    } else {
        idx = player.curr_idx + 1;
    }
    return idx;
}

int event_prev(void)
{
    int idx;
    if (player.shuffle) {
        player.shuffle_idx--;
        if (player.shuffle_idx < 0) {
            player.shuffle_idx = 0;
        }
        idx = player.order[player.shuffle_idx];
    } else {
        idx = player.curr_idx - 1;
    }
    return idx;
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
            if (player.shuffle) {
                player.shuffle = false;
            }
            int idx = ui.menu.offset_idx + ui.curs.y - 1;
            event_playsong(idx);
            clear_window(ui.view.w);
            draw_viewer();
            wrefresh(ui.view.w);
            break;
        }
        case KEY_LEFT: {
            if (player.playing) {
                mpv_seek(-5);
            }
            break;
        }
        case KEY_RIGHT: {
            if (player.playing) {
                mpv_seek(5);
            }
            break;
        }
        case '+':
        case '=': {
            mpv_volume(5);
            break;
        }
        case '-': {
            mpv_volume(-5);
            break;
        }
        case ',': {
            if (!player.playing) {
                break;
            }
            int idx = event_prev();
            event_playsong(idx);
            clear_window(ui.view.w);
            draw_viewer();
            wrefresh(ui.view.w);
            break;
        }
        case '.': {
            if (!player.playing) {
                break;
            }
            int idx = event_next();
            if (idx == -1) {
                break;
            }
            event_playsong(idx);
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
        case 'a': {
            player.autoplay = !player.autoplay;
            clear_window(ui.view.w);
            draw_viewer();
            wrefresh(ui.view.w);
            break;
        }
        case 's': {
            event_shuffle();
            event_playsong(player.order[player.shuffle_idx]);
            clear_window(ui.view.w);
            draw_viewer();
            wrefresh(ui.view.w);
            break;
        }
        case 'q': {
            running = LOOP_STOP;
            break;
        }
        default: break;
    }
}

void event_eof_shuffle(void)
{
    player.shuffle_idx++;
    if (player.shuffle_idx >= (int)songarr->size) {
        player.shuffle = false;
        player.autoplay = false;
        player.playing = false;
    } else {
        event_playsong(player.order[player.shuffle_idx]);
    }
}

void handle_mpv_properties(void)
{
    MPVProp p = mpv_property(fds[0].fd);
    if (p == PROP_EOF) {
        /* End of song reached */
        if (player.shuffle) {
            event_eof_shuffle();
        } else if (player.autoplay) {
            event_playsong(player.curr_idx+1);
        } else {
            player.playing = false;
        }
        clear_window(ui.view.w);
        draw_viewer();
        wrefresh(ui.view.w);
    }
}

void event_loop(void)
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

void cleanup(void)
{
    if (ncurses_initialized) {
        ui_destroy();
    }
    if (mpv_initialized) {
        mpv_terminate();
    }
    if (player_initialized) {
        free(player.order);
    }
    if (songarr_initialized) {
        songarr_destroy(songarr);
    }
}

void handle_sigint(int sig)
{
    (void)sig;
    running = LOOP_STOP;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <music-dirname>\n", argv[0]);
        return 1;
    }
    srand((unsigned) time(NULL));

    /* Setup SIGINT handler */
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Error, sigaction failed\n");
        return 1;
    }

    /* Build song playlist */
    songarr = songarr_init(argv[1]);
    if (songarr == NULL) {
        fprintf(stderr, "Error reading from directory: %s\n", argv[1]);
        return 1;
    }
    songarr_initialized = true;

    /* Setup player struct */
    if (!player_init(songarr->size)) {
        fprintf(stderr, "Error initializing player\n");
        cleanup();
        return 1;
    }
    player_initialized = true;

    /* Initialize MPV */
    int mpv_fd = mpv_init();
    if (mpv_fd == -1) {
        fprintf(stderr, "Error initializing MPV\n");
        cleanup();
        return 1;
    }
    mpv_initialized = true;

    /* Setup polling. */
    fds[0].fd = mpv_fd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    /* Initialize ncurses */
    if (!ui_init_core()) {
        fprintf(stderr, "Error initializing MPV\n");
        cleanup();
        return 1;
    }
    if (!ui_init_windows()) {
        fprintf(stderr, "Error initializing MPV\n");
        endwin();
        cleanup();
        return 1;
    }
    ncurses_initialized = true;

    event_loop();

    cleanup();
    return 0;
}

