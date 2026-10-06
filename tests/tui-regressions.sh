#!/bin/bash
# TUI regressions from the v1 final review (findings 1, 3, 6, 8). Needs tmux. Run from the repo root after make: bash tests/tui-regressions.sh
cd "$(dirname "$0")/.."
S="tmux -L dewrepro"
fresh() { $S kill-server 2>/dev/null; sleep 0.6; T=$(mktemp -d); mkdir -p $T/cfg/dew $T/data/dew; printf 'notify = 0\nbell = 0\n' > $T/cfg/dew/config; }
launch() { # $1 = TERM, $2 = extra env
    $S new-session -d -x 100 -y 30 "env TERM=$1 $2 XDG_DATA_HOME=$T/data XDG_CONFIG_HOME=$T/cfg ./dew; sleep 30"; sleep 1; }
verdict() { if [ "$2" = yes ]; then echo "PASS $1"; else echo "FAIL $1"; fi; }

# 1: a 300-character title keeps the note written in $EDITOR
fresh
LONG=$(printf 'x%.0s' $(seq 300))
printf '# Today\n- [ ] %s\n\n# Backlog\n' "$LONG" > $T/data/dew/tasks.md
printf '#!/bin/sh\necho "the note" > "$1"\n' > $T/ed; chmod +x $T/ed
launch xterm-256color "EDITOR=$T/ed"
$S send-keys n; sleep 1
grep -q '^  the note$' $T/data/dew/tasks.md && verdict "#1 long-title note saved" yes || verdict "#1 long-title note saved" no

# 3: with 8 colors the selected row is reversed and borders aren't black-on-black
fresh
printf '# Today\n- [ ] alpha\n- [ ] beta\n\n# Backlog\n' > $T/data/dew/tasks.md
launch xterm ""
$S send-keys j; sleep 0.5
ROW=$($S capture-pane -e -p | sed -n '3p')
TOP=$($S capture-pane -e -p | sed -n '1p')
rev=no; [[ "$ROW" == *$'\e[7m'* ]] && rev=yes
blk=no; [[ "$TOP" == *$'\e[30;40m'* || "$TOP" == *$'\e[30m\e[40m'* ]] && blk=yes
[ $rev = yes ] && [ $blk = no ] && verdict "#3 8-color selection/borders visible" yes || verdict "#3 8-color selection/borders visible (reverse=$rev black-border=$blk)" no

# 6: delete prompt opened on alpha; alpha removed outside; 'y' must not delete beta
fresh
printf '# Today\n- [ ] alpha\n- [ ] beta\n\n# Backlog\n' > $T/data/dew/tasks.md
launch xterm-256color ""
$S send-keys d; sleep 0.3
printf '# Today\n- [ ] beta\n- [ ] gamma\n\n# Backlog\n' > $T/data/dew/tasks.md.new && mv $T/data/dew/tasks.md.new $T/data/dew/tasks.md
sleep 1.5; $S send-keys y; sleep 0.5
grep -q 'beta' $T/data/dew/tasks.md && verdict "#6 prompt acts on the task it was opened for" yes || verdict "#6 prompt acts on the task it was opened for" no

# 8: while tasks.md is broken, 'x' must not change anything silently
fresh
printf '# Today\n- [ ] alpha\n\n# Backlog\n' > $T/data/dew/tasks.md
launch xterm-256color ""
printf '# Today\n- [ ] alpha <!-- dew waves=x -->\n\n# Backlog\n' > $T/data/dew/tasks.md.new && mv $T/data/dew/tasks.md.new $T/data/dew/tasks.md
sleep 1.5; $S send-keys x; sleep 0.4
LINE=$($S capture-pane -p | sed -n '2p')
[[ "$LINE" == *'[ ] '*'alpha'* ]] && verdict "#8 no silent change while file is broken" yes || verdict "#8 no silent change while file is broken" no

$S kill-server 2>/dev/null
