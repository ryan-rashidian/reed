# reed

TUI music/audio player (MPV wrapper).

Wraps MPV process (communicating over a UDS), does not use `libmpv`.

Written in C with the `ncurses` library.

**Work in Progress**: Features may break, be replaced, and bugs may be encountered. This is a personal project for fun/learning, and I may keep working on it in the future.

**Compatibility**: Not cross-platform. Only works on GNU/Linux (Potentially other POSIX compliant operating systems). Also depends on an installation of MPV.

## Features

- Menu scrolling without using the `menu.h` library.
- Automatic window re-sizing.
- Live updated Terminal-UI.

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
> Non-audio files may be registered by reed as an audio file in the menu. Until this is fixed, do not mix file types in the directory given to reed (and in any of its child directories). This program is not intended to be used with other multimedia file types, even if MPV supports them.

```bash
# Give the path to your music/playlist directory:
reed ~/media/music
# Or alternatively:
cd media/music
reed playlist1
```

## Controls

Scroll up:
- ARROW_UP
- 'k'
Scroll down:
- ARROW_DOWN
- 'j'
Scroll top:
- 'G'
Scroll bottom:
- 'g'
Select/Play:
- ENTER/RETURN
Pause toggle:
- SPACE
- 'p'
Auto-play toggle:
- 'a'
Quit:
- 'q'

## To-Do

- Help option
- Playlist/directory organization
- Shuffle
- Only load audio files into the menu; ignore other types.
- Better error handling/safety.
- Controls: FF, RW, NEXT, PREV, VOL+, VOL-

## Ideas

- Render time-stamp.
- Colors and attributes.

