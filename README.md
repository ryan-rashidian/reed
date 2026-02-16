# reed

TUI music/audio player (MPV wrapper).

Wraps MPV process (communicating over a UDS), does not use `libmpv`.

Written in C with the `ncurses` library.

**Work in Progress**: Features may break, be replaced, and bugs may be encountered. This is a personal project for fun/learning, and I may keep working on it in the future.

**Compatibility**: Not cross-platform. Only works on GNU/Linux (Potentially other POSIX compliant operating systems). Also depends on an installation of MPV.

## Features

- Shuffle/Auto-play
- Menu scrolling (without `menu.h`)
- Automatic window re-sizing
- Live updated Terminal-UI

## Build

```bash
git clone https://github.com/ryan-rashidian/reed.git
cd reed
make
# Then move the executable where you want it.
# In your users PATH via ~/bin for example:
mv reed ~/bin/reed
```

## Usage

> [!IMPORTANT]
> Non-audio-files may be registered by reed as an audio-file in the menu. Until this is fixed, do not mix file types in the directory given to reed (and in any of its child directories). This program is not intended to be used with other multimedia file types, even if MPV supports them.

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

