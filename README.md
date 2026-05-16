# reed

MPV music player TUI implemented with `ncurses`.

Wraps MPV process (communicating over a UDS), does not use `libmpv`.

Written in C with the `ncurses` library.

**Work in Progress**: Features may break, be replaced, and bugs may be encountered. This is a personal project for fun and learning.

**Compatibility**: Not cross-platform. Only works on POSIX compliant systems. Also depends on an installation of MPV.

## Features

- Shuffle/Auto-play
- Menu scrolling (without depending on `menu.h`)
- Automatic window re-sizing
- Live updated Terminal-UI

## Build

```bash
git clone https://github.com/ryan-rashidian/reed.git
cd reed
make
```

## Usage

> [!IMPORTANT]
> Non-audio-files may be registered by reed as an audio-file in the menu. Until this is fixed, do not mix file types in the directory supplied to reed (and in any of its child directories). This program is not intended to be used with other multimedia file types, even if MPV supports them.

```bash
# Give the path to your music/playlist directory:
reed ~/media/music
# Or alternatively:
cd media/music
reed playlist1
```

## Controls

| Action | Key |
| --- | --- |
| Scroll+ | `ARROW_UP` / `k` |
| Scroll- | `ARROW_DOWN` / `j` |
| Scroll Top | `g` |
| Scroll Bottom | `G` |
| Select/Play | `ENTER` (`RETURN`) |
| Pause (Toggle) | `SPACE` / `p` |
| Autoplay (Toggle) | `a` |
| Shuffle | `s` |
| VOL+ | `+` / `=` |
| VOL- | `-` |
| SEEK+ | `ARROW_RIGHT` |
| SEEK- | `ARROW_LEFT` |
| NEXT | `.` |
| PREV | `,` |
| Quit | `q` |

## To-Do

- Help option
- Playlist/directory organization
- Check file type; accept only audio-file.

## Ideas

- Render time-stamp.
- Colors and attributes.

