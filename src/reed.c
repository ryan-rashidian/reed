/* 
 * reed - MPV music player TUI implementated with ncurses.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ncurses.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include "mpvproc.h"
#include "songarr.h"

#define TITLE_MENU "> Songs <"
#define SUBTITLE_MENU "> ('q' - quit) reed 0.5.0 <"
#define TITLE_VIEW "> Playing <"
#define MAX_SONGTITLE_LEN 512

#define BORDER_SIZE 2
#define CURSOR_SIZE 3

enum {
    LOOP_STOP,
    LOOP_RUN
};

typedef struct {
    bool playing;
    bool paused;
    bool autoplay;
    bool shuffle;
    int *order;
    int shuffle_idx;
    int curr_idx;
    char curr_track[MAX_SONGTITLE_LEN+1];
} PlayerState;

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

typedef struct {
    Menu menu;
    View view;
    RowCol max;
    RowCol curs;
} UI;

bool songarr_initialized = false;
bool player_initialized  = false;
bool mpv_initialized     = false;
bool ncurses_initialized = false;

static volatile sig_atomic_t running = LOOP_RUN;
static SongArr *song_arr;
static struct pollfd fds[2];

static PlayerState player = {
    .paused = false,
    .autoplay = false,
    .shuffle = false,
    .curr_track[0] = '\0'
};
static UI ui = { .curs = {1, 2}, .menu.offset_idx = 0 };

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

void ui_init_colors(void)
{
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_BLUE,    COLOR_BLACK);
        init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    }
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

void draw_menu_items(void)
{
    int max_cols = ui.menu.max.x - BORDER_SIZE - CURSOR_SIZE;
    int max_rows = ui.menu.max.y - BORDER_SIZE;
    int j = ui.menu.offset_idx;
    wattrset(ui.menu.w, COLOR_PAIR(1));
    for (int row = 0; row < max_rows && j < (int)song_arr->size; row++, j++) {
        const char *name = song_arr->arr[j].name;
        int str_len = strlen(name);
        if (str_len > max_cols) {
            char short_name[max_cols+1];
            snprintf(short_name, max_cols+1, "%s", name);
            mvwprintw(ui.menu.w, row+1, 1, " > %s", short_name);
        } else {
            mvwprintw(ui.menu.w, row+1, 1, " > %s", name);
        }
    }
    wattroff(ui.menu.w, COLOR_PAIR(1));
}

void draw_menu_titles(void)
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
}

void draw_menu(void)
{
    draw_menu_titles();
    draw_menu_items();
}

void draw_viewer_title(void)
{
    int x = ui.view.max.x;
    int offset;

    int title_len = strlen(TITLE_VIEW);
    offset = (title_len/2) + (title_len%2);
    int title_ctr_x = x/2 - offset;
    mvwprintw(ui.view.w, 0, title_ctr_x, "%s", TITLE_VIEW);
}

void draw_viewer_track_name(void)
{
    int y = ui.view.max.y;
    int x = ui.view.max.x;
    int max_cols = x - BORDER_SIZE;
    const char *name = player.curr_track;
    int track_len = strlen(name);

    wattrset(ui.view.w, COLOR_PAIR(1) | A_BOLD);
    if (track_len > max_cols) {
        char short_name[max_cols+1];
        snprintf(short_name, max_cols+1, "%s", name);
        mvwprintw(ui.view.w, y/2, 1, "%s", short_name);
    } else {
        int offset = (track_len/2) + (track_len%2);
        int track_ctr_x = x/2 - offset;
        mvwprintw(ui.view.w, y/2, track_ctr_x, "%s", name);
    }
    wattroff(ui.view.w, COLOR_PAIR(1) | A_BOLD);
}

void draw_viewer_shuffle_indicator(void)
{
    int y = ui.view.max.y;
    int x = ui.view.max.x;
    int ctr_x = x/2 - (strlen("[Shuffle]") / 2) - 1;

    wattrset(ui.view.w, COLOR_PAIR(2));
    mvwprintw(ui.view.w, y-2, ctr_x, "[Shuffle]");
    wattroff(ui.view.w, COLOR_PAIR(2));
}

void draw_viewer_autoplay_indicator(void)
{
    int y = ui.view.max.y;
    int x = ui.view.max.x;
    int ctr_x = x/2 - (strlen("[Auto-Play]") / 2) - 1;

    wattrset(ui.view.w, COLOR_PAIR(2));
    mvwprintw(ui.view.w, y-2, ctr_x, "[Auto-Play]");
    wattroff(ui.view.w, COLOR_PAIR(2));
}

void draw_viewer_paused_indicator(void)
{
    int y = ui.view.max.y;
    int x = ui.view.max.x;
    int ctr_x = x/2 - strlen("> PAUSE <") - 1;

    mvwprintw(ui.view.w, y-1, ctr_x, "> PAUSE <");
}

void draw_viewer(void)
{
    draw_viewer_title();
    if (player.playing) {
        draw_viewer_track_name();
    }
    if (player.shuffle) {
        draw_viewer_shuffle_indicator();
    } else if (player.autoplay) {
        draw_viewer_autoplay_indicator();
    }
    if (player.paused) {
        draw_viewer_paused_indicator();
    }
}

void resize_items(void)
{
    if (ui.menu.offset_idx != 0) {
        int max_rows = ui.max.y - BORDER_SIZE;
        if (max_rows >= (int)song_arr->size) {
            /* Reset offset index if window is large enough */
            ui.menu.offset_idx = 0;
        } else {
            /* Show more items if window is large enough */
            int diff = (int)song_arr->size - max_rows;
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
    int max_rows = ui.max.y - BORDER_SIZE;
    int diff = (int)song_arr->size - max_rows;
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
    int max_rows = ui.max.y - BORDER_SIZE;
    int items = (int)song_arr->size;
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
    int max_rows = ui.max.y - BORDER_SIZE;
    if (max_rows > (int)song_arr->size) {
        ui.curs.y = song_arr->size;
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
    int max_rows = ui.max.y - BORDER_SIZE;
    if (ui.curs.y > max_rows) {
        ui.curs.y = max_rows;
    }
    if (ui.curs.y < 1) {
        ui.curs.y = 1;
    }
    wmove(ui.menu.w, ui.curs.y, ui.curs.x);
}

int validate_idx(int idx)
{
    if (idx >= (int)song_arr->size) {
        return -1;
    } else if (idx < 0) {
        idx = 0;
    }
    if (player.shuffle) {
        player.shuffle_idx = idx;
        idx = player.order[idx];
    }
    player.curr_idx = idx;
    return idx;
}

void event_playsong(int idx) 
{
    if ((idx = validate_idx(idx)) == -1) {
        return;
    }
    mpv_load_song(song_arr->arr[idx].path);
    player.playing = true;
    strncpy(
        player.curr_track,
        song_arr->arr[idx].name,
        (size_t)MAX_SONGTITLE_LEN
    );
}

void event_shuffle(void)
{
    for (int i = (int)song_arr->size - 1; i > 0; i--) {
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
        idx = player.shuffle_idx + 1;
    } else {
        idx = player.curr_idx + 1;
    }
    return idx;
}

int event_prev(void)
{
    int idx;
    if (player.shuffle) {
        idx = player.shuffle_idx - 1;
    } else {
        idx = player.curr_idx - 1;
    }
    return idx;
}

void handle_key_resize(void)
{
    update_maxyx();
    resize_windows();
    resize_items();
    clear_window(ui.menu.w);
    clear_window(ui.view.w);
    draw_menu();
    draw_viewer();
    refresh_windows();
}

void handle_key_enter(void)
{
    if (player.shuffle) {
        player.shuffle = false;
    }
    int idx = ui.menu.offset_idx + ui.curs.y - 1;
    event_playsong(idx);
    clear_window(ui.view.w);
    draw_viewer();
    wrefresh(ui.view.w);
}

void handle_key_left(void)
{
    if (player.playing) {
        mpv_seek(-5);
    }
}

void handle_key_right(void)
{
    if (player.playing) {
        mpv_seek(5);
    }
}

void handle_prev_song(void)
{
    if (!player.playing) {
        return;
    }
    int idx = event_prev();

    event_playsong(idx);
    clear_window(ui.view.w);
    draw_viewer();
    wrefresh(ui.view.w);
}

void handle_next_song(void)
{
    if (!player.playing) {
        return;
    }
    int idx = event_next();
    if (idx >= (int)song_arr->size) {
        return;
    }

    event_playsong(idx);
    clear_window(ui.view.w);
    draw_viewer();
    wrefresh(ui.view.w);
}

void handle_toggle_pause(void)
{
    mpv_cycle_pause();
    player.paused = !player.paused;
    clear_window(ui.view.w);
    draw_viewer();
    wrefresh(ui.view.w);
}

void handle_toggle_autoplay(void)
{
    player.autoplay = !player.autoplay;
    clear_window(ui.view.w);
    draw_viewer();
    wrefresh(ui.view.w);
}

void handle_toggle_shuffle(void)
{
    event_shuffle();
    event_playsong(player.shuffle_idx);
    clear_window(ui.view.w);
    draw_viewer();
    wrefresh(ui.view.w);
}

void switch_keypress(int key)
{
    switch (key) {
        case KEY_RESIZE: {
            handle_key_resize();
        } break;
        case 'k':
        case KEY_UP: {
            cursor_scroll_up();
        } break;
        case 'g': {
            cursor_scroll_top();
        } break;
        case 'j':
        case KEY_DOWN: {
            cursor_scroll_down();
        } break;
        case 'G': {
            cursor_scroll_bottom();
        } break;
        case '\n':
        case KEY_ENTER: {
            handle_key_enter();
        } break;
        case KEY_LEFT: {
            handle_key_left();
        } break;
        case KEY_RIGHT: {
            handle_key_right();
        } break;
        case '+':
        case '=': {
            mpv_volume(5);
        } break;
        case '-': {
            mpv_volume(-5);
        } break;
        case ',': {
            handle_prev_song();
        } break;
        case '.': {
            handle_next_song();
        } break;
        case ' ':
        case 'p': {
            handle_toggle_pause();
        } break;
        case 'a': {
            handle_toggle_autoplay();
        } break;
        case 's': {
            handle_toggle_shuffle();
        } break;
        case 'q': {
            running = LOOP_STOP;
        } break;
        default: break;
    }
}

void eof_event_shuffle(void)
{
    int idx = player.shuffle_idx + 1;
    if (idx >= (int)song_arr->size) {
        player.playing = false;
        player.shuffle = false;
    } else {
        event_playsong(player.shuffle_idx+1);
    }
}

void eof_event_autoplay(void)
{
    int idx = player.curr_idx + 1;
    if (idx >= (int)song_arr->size) {
        player.playing = false;
    } else {
        event_playsong(player.curr_idx+1);
    }
}

void handle_mpv_properties(void)
{
    MPVProp p = mpv_property(fds[0].fd);
    if (p == PROP_EOF) {
        /* End of song reached */
        if (player.shuffle) {
            eof_event_shuffle();
        } else if (player.autoplay) {
            eof_event_autoplay();
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
        song_arr_destroy(song_arr);
    }
}

void handle_sigint(int sig)
{
    (void)sig;
    running = LOOP_STOP;
}

bool init_signal_handler(void)
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Error, sigaction failed\n");
        return false;
    }
    return true;
}

bool init_song_playlist(const char *dir_name)
{
    song_arr = song_arr_init(dir_name);
    if (song_arr == NULL) {
        fprintf(stderr, "Error reading from directory: %s\n", dir_name);
        return false;
    }
    songarr_initialized = true;
    return true;
}

bool init_player(void)
{
    if (!player_init(song_arr->size)) {
        fprintf(stderr, "Error initializing player\n");
        return false;
    }
    player_initialized = true;
    return true;
}

bool init_mpv_proc(void)
{
    int mpv_fd = mpv_init();
    if (mpv_fd == -1) {
        fprintf(stderr, "Error initializing MPV\n");
        return false;
    }

    fds[0].fd = mpv_fd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    mpv_initialized = true;
    return true;
}

bool init_ncurses(void)
{
    if (!ui_init_core()) {
        fprintf(stderr, "Error initializing MPV\n");
        return false;
    }
    if (!ui_init_windows()) {
        fprintf(stderr, "Error initializing MPV\n");
        endwin();
        return false;
    }
    ui_init_colors();
    ncurses_initialized = true;
    return true;
}

bool init_reed(char *argv[])
{
    atexit(cleanup);
    srand((unsigned) time(NULL));
    if (!init_signal_handler())       return false;
    if (!init_song_playlist(argv[1])) return false;
    if (!init_player())               return false;
    if (!init_mpv_proc())             return false;
    if (!init_ncurses())              return false;
    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <music-dirname>\n", argv[0]);
        return 1;
    }
    if (!init_reed(argv)) return 2;

    event_loop();

    return 0;
}

