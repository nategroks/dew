# dew

dew is to-do app like no other.

A terminal to-do list you knock out with a pomodoro timer. A pomodoro is a
**wave**, counted with the water rune ᛚ. Next to your list runs a Game of Life
board in soft Nord colors that reacts to your work: starting a wave seeds it,
checking off a task launches a fleet of gliders and sends a pixel-art wolf
(frost, ember, aurora, shadow and dusk take turns) chasing the task's rune
across the board, finishing
a wave sets off a burst and plants a pattern in today's garden, and breaks
flood the board with a river (the white wolf gallops along its bank) into
Brian's Brain fireworks. The wolves are drawn in real pixels in kitty,
ghostty and foot.
Every task wears one of the 24 Elder Futhark runes in its own soft color
(`R` picks another); `tools/runes.py` holds the rune art.
Press `m` for 15, 25 or 45-minute waves (the breaks follow), and `r` to give
your waves a different automaton: HighLife, Day & Night, Seeds, Maze, Star
Wars, or any `B…/S…` rule.

```
┌ dew ── Today ───────────────────┐┌─── ᛚ 18:42 focus · 25 min · wave 4 ─┐
│▶ [ ] ᚱ fix grub theme        ᛚ2 ││  ⠀⣠⣄⠀⠀⠀⠀⢀⡀⠀⠀⠀⣀⠀⠀⠀⠀⠀⣠⣄⠀⠀⠀⢀⡀⠀⠀ │
│  [ ] ᚷ email site about HM      ││  ⠀⠙⠋⠀⢀⣴⡄⠈⠁⠀⣰⡆⠛⠀⠀⣤⠀⠀⠙⠋⢀⣴⡄⠈⠁⠀⣰ │
│  [x] ᚹ polymon P0 scaffold   ᛚ3 ││  ⠀⠀⠀⠀⠈⠛⠁⠀⠀⠀⠙⠃⠀⠀⠀⠛⠀⠀⠀⠀⠈⠛⠁⠀⠀⠀⠙ │
│── Backlog (4) ──────────────────││                                      │
└─────────────────────────────────┘└─────────────────────── B3/S23 conway ─┘
```

## Build

Needs a C11 compiler and ncursesw.

```sh
make                          # builds ./dew
make test                     # unit tests
make debug                    # tests under ASan + UBSan
make install PREFIX=~/.local  # dew, man page, tldr page
```

## Use

```sh
dew                           # open the list
dew add -t fix the grub theme # add to Today from the shell
man dew                       # keys, file format, config
```

Tasks are a plain Markdown checklist in `~/.local/share/dew/tasks.md`.
Edit it by hand any time; dew reloads it.

Works in foot and kitty (exact Nord colors via palette redefinition).
Ghostty should work too; it hasn't been tested yet.

## License

GPL-2.0, see `LICENSE`.
