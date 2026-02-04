# reed

TUI music/audio player (MPV wrapper).

Wraps MPV process (communicating over a UDS), does not use `libmpv`.

Written in C with the `ncurses` library.

**Work in Progress**: Features may break, be replaced, and bugs may be encountered. This is a personal project for fun/learning, and I may keep refining/adding to it in the future.

**Compatibility**: Not cross-platform. Only works on GNU/Linux (Potentially other POSIX compliant operating systems). Also depends on an installation of MPV.

## Features

- Menu scrolling without using `menu.h` library.
- Window re-sizing.
- Live updated Terminal-UI. (Render on MPV events: only end-of-song for now, but may poll in the future for next track, or playback time.)

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

- Scroll down:   ARROW_DOWN or 'j'
- Scroll up:     ARROW_UP or 'k'
- Scroll bottom: 'g'
- Scroll top:    'G'
- Select/Play:   ENTER
- Pause cycle:   SPACE or 'p'
- Quit:          'q'

## To-Do

- Help option
- Playlist tab
- Shuffle/Auto-play
- Only load audio files into the menu; ignore other types.
- Better error handling, safety.
- Controls: FF, RW, NEXT, PREV
- Clean up code/readability.

## Ideas

- Render time-stamp.
- Colors and attributes.
- Organize songs by artist/directory. By directory is simple and avoids parsing file-name variations (e.g. "1. Artist - Song", "Artist_Song", "Song - Album - Artist", etc.)

