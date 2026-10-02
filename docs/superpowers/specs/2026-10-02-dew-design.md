# dew — design spec

Date: 2026-10-02
Status: approved 2026-10-02; implementation plan in `docs/superpowers/plans/2026-10-02-dew-v1.md`

## 1. What dew is

dew is a terminal to-do list you knock out with a pomodoro timer, with a Game of Life
animation that reacts to your work. It takes the core loop of Super Productivity
(pick a task, focus on it, take a break) and drops everything else.

A pomodoro in dew is called a **wave**. The sprite is 🌊.

### Success criteria

- Adding, reordering, checking off and moving tasks between Today and Backlog takes one
  or two keystrokes and never loses data.
- A wave can be started on the selected task with one key. The timer, breaks and the
  per-task wave count work without thinking about them.
- The Life pane is fun to glance at: it reacts to starting a wave, finishing a task,
  finishing a wave, pausing and taking a break. Each day it builds a garden of the
  waves you finished.
- Runs correctly in foot and kitty. Ghostty is expected to work and is verified once
  it is installed.
- Ships a man page (`dew.1`) and a tldr page.

### Non-goals (v1)

Projects, tags, due dates, recurring tasks, subtasks, estimates, timesheets, worklog
export, stats, sync, mouse support, sound files, browsing past gardens.

"Very simple, no brain damage" is the governing rule: when in doubt, leave it out.

## 2. Screen and keys

```
┌ dew ── Today ─────────────────────┬─ 🌊 18:42 focus · wave 4 ─────┐
│ ▶ [ ] fix grub theme          🌊2 │ ⠀⠀⣠⣄⠀⠀⠀⠀⠀⢀⡀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀ │
│   [ ] email site about HM         │ ⠀⠀⠙⠋⠀⢀⣴⡄⠀⠈⠁⠀⠀⣰⡆⠀⠛⠀⠀⣤⠀ │
│   [x] polymon P0 scaffold     🌊3 │ ⠀⠀⠀⠀⠀⠈⠛⠁⠀⠀⠀⠀⠀⠙⠃⠀⠀⠀⠀⠛⠀ │
├ Backlog (4) ──────────────────────┤                               │
│   [ ] retheme sddm login          │                B3/S23 conway  │
├───────────────────────────────────┴───────────────────────────────┤
│ note: GRUB lives on /boot (noauto ESP). Mount it before editing.  │
└ a add  e edit  n note  t today↔backlog  x done  J/K move  ␣ wave  ┘
```

- **Left pane:** Today on top, Backlog below. `▶` marks the task the running wave is
  attached to. The wave count shows as `🌊N` (hidden when 0).
- **Right pane:** the Life board. The header shows timer and mode. The footer shows the
  current rule.
- **Note line:** the first line of the selected task's note, plus "(+N more lines)" when there are more.
- **Key line:** the keys below.

Below 80×24 the Life pane is hidden and the list uses the whole width. Resizing is
handled live.

| Key | Action |
|---|---|
| `j` `k` / `↓` `↑` | move selection (Today then Backlog, one continuous list) |
| `g` `G` | jump to top / bottom |
| `Tab` | jump between Today and Backlog |
| `a` | add a task to the focused list (inline prompt) |
| `e` | edit the title (inline prompt, prefilled) |
| `n` | edit the note in `$VISUAL` / `$EDITOR` (fallback `vi`) |
| `x` | toggle done |
| `t` | move between Today and Backlog |
| `J` `K` | move the task down / up within its list |
| `d` | delete, after a `y/n` prompt |
| `Space` | idle: start a wave on the selected task. Focus: pause. Paused: resume. Break: skip the break and start a wave |
| `s` | stop: finish the current wave now (counts it), or end a break |
| `S` | abandon the current wave (not counted, no garden plant) |
| `w` | full-screen garden view (any key returns) |
| `?` | help overlay |
| `q` | quit (a running wave is saved and resumes on next launch) |

Note: the earlier draft used `g` for the garden. The garden moves to `w` so `g`/`G`
can follow vi top/bottom.

The inline prompt supports printable text, `Backspace`, `←` `→`, `Home` `End`,
`Ctrl-U`, `Enter` to commit and `Esc` to cancel.

Done tasks stay in Today, drawn dim with `[x]` (curses has no strikethrough), for the rest of the day. On the first launch
(or tick) after midnight they move to a `# Done YYYY-MM-DD` section at the bottom of
the file.

## 3. Waves and the Life pane

### Timer

States: `idle → focus ⇄ paused`, `focus → break → idle`.

- Focus 25 min, short break 5 min, long break 15 min every 4th wave. All configurable.
- A finished wave adds 1 to the attached task's wave count and plants one pattern in
  today's garden.
- Break ends by itself and returns to idle. dew never starts the next wave on its own.
- Nudges at the end of focus and the end of a break: a terminal bell and, if enabled
  and `notify-send` exists, a desktop notification.
- Time comes from `CLOCK_MONOTONIC` while running. The wave end time is also saved as
  wall-clock time, so quitting and relaunching resumes the wave (or finishes it if it
  already ran out).

### Board

- Grid of 2×4 Life cells per terminal cell, drawn with braille (U+2800–U+28FF). The
  board is sized to the pane and keeps a hidden 8-cell margin that absorbs anything
  flying off the edge.
- Each cell has a state, an age and a tint. Newborn cells inherit the most common tint
  among their live neighbors, so fleets keep their color.
- A terminal cell can show one color, so each braille glyph takes the color of its
  youngest live dot (the most vivid).
- 20 fps while anything moves. In idle garden view it steps about 2 generations per
  second and redraws only then. Target: under 3% of one CPU core while animating.

### Rules

| Mood | Rule |
|---|---|
| focus | Conway B3/S23. About every 20 s there is a 45% chance of a 9 s HighLife (B36/S23) stretch |
| break | Brian's Brain (on → dying → off, born with exactly 2 on neighbors) |
| paused | Conway at a slow step, plus 3% random decay per frame, drawn in grays |
| idle | Conway on the garden only (still lifes stay put, oscillators pulse) |

In focus, if the population drops below a threshold, a small soup patch is added so
the board never goes empty.

### Events

| You do | Board does |
|---|---|
| start a wave | a short glitch tear, then a random soup seeds one band of the board |
| check off a task | a fleet of 5 gliders (diagonal) or 4 LWS spaceships (horizontal) launches in the next tint. In idle it runs fast for a few seconds over the garden, then the garden is restored |
| finish a wave | an R-pentomino, acorn or diehard burst plus a glitch tear. About 2 s later the break starts |
| break starts | a wave front sweeps left to right, clearing the board with 🌊 riding the crest, then Brian's Brain blobs are seeded |
| break ends | another sweep, then the garden |
| pause | grays out and decays |

**Glitch effects** (from the user's `glitch` program): a *tear* is 3–6 frames where
random rows shift sideways by up to 3 columns and about 36 random cells show `@#%*&10SE`
static. *Jitter* is a 0.12% per-dot chance per frame of drawing a glyph instead of dots.
`glitch = 0` in the config turns both off.

### Garden

Every finished wave plants one pattern at a free spot (no overlap, 8-cell padding so oscillators never touch):
still lifes and small oscillators, with a pentadecathlon on every 4th wave and a
pulsar on every 8th. The tint cycles per wave. Positions live in a fixed 120×64 space and are scaled
onto whatever board is showing. The garden is per day and saved to disk.
Past days are kept but not browsable in v1.

### Palette (Nord)

| Use | Colors |
|---|---|
| background, banding | nord0 `#2e3440`, nord1 `#3b4252` |
| age ramp, newborn → old | nord6 `#eceff4` → nord13 `#ebcb8b` → nord7 `#8fbcbb` → nord8 `#88c0d0` → nord9 `#81a1c1` → nord10 `#5e81ac` → nord2 `#434c5e` |
| fleet / garden tints | nord12 `#d08770`, nord14 `#a3be8c`, nord15 `#b48ead`, nord11 `#bf616a`, nord13 `#ebcb8b` |
| Brian's Brain dying cells | nord15 |
| sweep foam | nord6, nord4, nord8, nord7 |
| UI text | nord4 / nord6, labels nord8, dim nord3 |

Rows are banded every 4 terminal rows with a faint nord1 tint, as glitch does.
`~/.config/dew/colors` can override any `nordN` value.

The reference look is the throwaway browser mock in `docs/preview/life-pane.html`.

## 4. Data files and module layout

### Files

| Path | Contents |
|---|---|
| `$XDG_DATA_HOME/dew/tasks.md` (default `~/.local/share/dew/tasks.md`) | the tasks |
| `$XDG_DATA_HOME/dew/state` | running wave: mode, task title, end time (wall clock), waves today |
| `$XDG_DATA_HOME/dew/garden/YYYY-MM-DD` | one line per finished wave: `HH:MM pattern x y tint` |
| `$XDG_DATA_HOME/dew/lock` | `flock` single-instance lock for the TUI |
| `$XDG_CONFIG_HOME/dew/config` | settings |
| `$XDG_CONFIG_HOME/dew/colors` | optional Nord overrides |

### tasks.md format

Plain Markdown, readable and editable in any editor:

```markdown
# Today
- [ ] fix grub theme on the ESP <!-- dew waves=2 -->
  GRUB lives on /boot (noauto ESP).
  Mount it before editing.
- [x] polymon P0 scaffold <!-- dew waves=3 done=2026-10-02 -->

# Backlog
- [ ] retheme sddm login

# Done 2026-10-01
- [x] tv-underscan helper <!-- dew waves=1 done=2026-10-01 -->
```

Rules:
- `# Today` and `# Backlog` are the two lists. `# Done …` sections are archive only:
  kept and appended to, never shown.
- A task is `- [ ] ` or `- [x] ` plus the title. The optional trailing
  `<!-- dew key=value … -->` holds `waves` and `done`. Unknown keys are kept.
- Lines indented by two or more spaces directly under a task are its note (the indent
  is stripped on load and written back as two spaces).
- Any other line is kept verbatim in its position and never shown. dew never drops
  content it doesn't understand.
- Writes are atomic: write `tasks.md.tmp`, `fsync`, `rename`. The previous version is
  kept as `tasks.md.bak`.
- dew saves after every change, so it never holds unsaved edits. Once a second it checks
  the file's mtime and size. If someone else changed the file (for example in
  Spacemacs), it reloads and keeps the selection on the same title if it still exists.
- Files with Windows line endings load fine and are saved back with LF.
- A missing file is created with empty `# Today` and `# Backlog` sections. A file that
  fails to parse is never overwritten: dew exits with the line number.

### config

`key = value`, `#` comments, unknown keys warned about on stderr and ignored:

```
focus        = 25     # minutes
short_break  = 5
long_break   = 15
long_every   = 4
sprite       = wave   # wave (🌊) | ascii (≈) | bolt (⚡)
bell         = 1
notify       = 1      # notify-send at the end of focus and breaks
fps          = 20
glitch       = 1      # 0 turns off tears and jitter
```

### Command line

```
dew                     open the TUI
dew add [-t] TEXT…      add a task to the Backlog (-t: to Today) and exit
dew --file PATH         use another tasks file
dew --help | --version
```

There are two locks. The TUI holds `lock` for its whole run, so only one TUI runs at a
time. Every write to `tasks.md` (from the TUI or from `dew add`) holds a short `flock`
on `tasks.md.lock` (not on `tasks.md` itself, which `rename` replaces). `dew add` doesn't take the instance lock, so it works while the
TUI is open, and the TUI picks the change up through its mtime check.

### Modules (C11, one header per module)

The pure modules have no ncurses and no clock reads, so they can be unit-tested.

| Module | Does | Pure |
|---|---|---|
| `tasks` | task list model: add, edit, delete, move, reorder, toggle, archive rollover | yes |
| `taskfile` | parse and serialize `tasks.md`, including raw passthrough lines | yes |
| `store` | atomic save, `.bak`, mtime watch, lock, XDG paths | no |
| `wave` | timer state machine, driven by `now` passed in | yes |
| `life` | grid, ages, tints, rules (Conway, HighLife, Brian's Brain), step | yes |
| `patterns` | pattern library (RLE-ish strings), flips, stamping | yes |
| `director` | turns events into board effects: soup, fleet, burst, sweep, tear, decay, keep-alive | yes (takes an RNG seed) |
| `garden` | plant placement, load and save per-day files | placement pure, I/O separate |
| `palette` | Nord colors, ramp tables, `init_color` slots or nearest-256 fallback | mostly |
| `render` | ncurses drawing of all panes, braille packing, glitch offsets | no |
| `input` | keys → actions, inline prompt, `$EDITOR` handoff | no |
| `config` | config and colors parsing | yes |
| `nudge` | bell and `notify-send` via `posix_spawnp` | no |
| `main` | arguments, setup, main loop, signal and resize handling | no |

Main loop: `wtimeout` based. The timeout is the time until the next animation frame,
or until the next garden step / timer second when nothing moves. `KEY_RESIZE` recomputes
the layout and resizes the board, keeping cells that still fit. `SIGINT`/`SIGTERM` save
state and restore the terminal.

### Color handling

- If `can_change_color()` and the terminal has at least 256 colors (foot and kitty
  both qualify): redefine slots 16 up to about 80 with the exact Nord, ramp and gray
  values, and restore the terminal's palette on exit.
- Otherwise use the nearest xterm-256 color.
- Otherwise (8/16 colors): plain ANSI colors, no ramp.
- `sprite = ascii` plus no color still gives a usable list and timer, for the Linux
  console.

## 5. Build, tests, docs

- **Build:** a plain `Makefile`, like glitch. `make`, `make test`,
  `make debug` (ASan + UBSan), `make install PREFIX=~/.local`.
  Requires ncursesw (`pkg-config ncursesw`). No other dependencies.
- **Flags:** `-std=c11 -Wall -Wextra -Wpedantic -Werror -O2`. Feature-test macros come only from
  `pkg-config --cflags ncursesw` (`-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600`); defining our own clashes.
- **Unit tests:** a single test binary built from `tests/*.c` with a small assert macro,
  no framework. Covers:
  - `taskfile`: round-trip (parse → serialize gives identical bytes), notes, meta
    comments, unknown lines kept, malformed input reports a line number
  - `tasks`: every operation, plus archive rollover across midnight
  - `wave`: every transition with a fake clock, long-break cadence, resume after a
    restart that happened mid-wave or after the end time
  - `life`: blinker has period 2, a glider moves (1,1) every 4 generations, a block
    is stable, HighLife B6 births, Brian's Brain transitions, the margin absorbs gliders
  - `garden`: plants never overlap or leave the board, and the file round-trips
  - `config`: defaults, overrides, bad values
- **Manual checks (in the plan as a checklist):** foot and kitty: exact Nord colors and
  palette restored after quit, braille and 🌊 render, resize down to 79×23 and back,
  `n` opens Spacemacs/`$EDITOR` and returns cleanly, a second `dew` refuses to start,
  `dew add` while the TUI runs, CPU use while animating. Ghostty: same list once
  installed.
- **Docs:**
  - `dew.1` man page (NAME, SYNOPSIS, DESCRIPTION, KEYS, WAVES, FILES with the
    tasks.md format, CONFIGURATION, ENVIRONMENT, EXAMPLES), installed to
    `$PREFIX/share/man/man1`.
  - `docs/tldr/dew.md` in the same style as glitch's tldr page, installed to
    `~/.config/tldr/pages/common/dew.md` by `make install`.
  - A short `README.md`.

## 6. Open items

- License: GPL-2.0 (the `LICENSE` in the GitHub repo).
- Repo: https://github.com/nategroks/dew
- Ghostty: verify when installed.
