# dew

dew is to-do app like no other.

A terminal to-do list you knock out with a pomodoro timer. A pomodoro is a
**wave**, counted with the water rune ᛚ. Next to your list runs a Game of Life
board in Nord colors that reacts to your work: starting a wave seeds it,
checking off a task launches a fleet of gliders, finishing a wave sets off a
burst and plants a pattern in today's garden, and breaks flood the board with
a river (a white pixel-art wolf gallops along its bank, drawn in real pixels
in kitty, ghostty and foot) into Brian's Brain fireworks.
Press `r` to give your waves a different automaton: HighLife, Day & Night,
Seeds, Maze, Star Wars, or any `B…/S…` rule.

```
┌ dew ── Today ───────────────────┐┌──────────── ᛚ 18:42 focus · wave 4 ─┐
│▶ [ ] fix grub theme          ᛚ2 ││  ⠀⣠⣄⠀⠀⠀⠀⢀⡀⠀⠀⠀⣀⠀⠀⠀⠀⠀⣠⣄⠀⠀⠀⢀⡀⠀⠀ │
│  [ ] email site about HM        ││  ⠀⠙⠋⠀⢀⣴⡄⠈⠁⠀⣰⡆⠛⠀⠀⣤⠀⠀⠙⠋⢀⣴⡄⠈⠁⠀⣰ │
│  [x] polymon P0 scaffold     ᛚ3 ││  ⠀⠀⠀⠀⠈⠛⠁⠀⠀⠀⠙⠃⠀⠀⠀⠛⠀⠀⠀⠀⠈⠛⠁⠀⠀⠀⠙ │
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
