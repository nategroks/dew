#!/bin/bash
# Runs ./dew through a break in a pty that reports pixel sizes, as kitty and as
# foot, and checks the wolf's graphics output. Prints PASS/FAIL per check.
cd "$(dirname "$0")/.." || exit 1
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fails=0
check() { if [ "$2" = yes ]; then echo "PASS $1"; else echo "FAIL $1"; fails=$((fails + 1)); fi; }
count() { python3 -c "import sys; print(open(sys.argv[1],'rb').read().count(sys.argv[2].encode().decode('unicode_escape').encode('latin1')))" "$1" "$2"; }

python3 tests/ptyrun.py xterm-kitty "$T/kitty" >>"$T/log"
check "kitty: six frames uploaded once" "$([ "$(count "$T/kitty" '\x1b_Ga=t')" = 6 ] && echo yes)"
check "kitty: the wolf is placed" "$([ "$(count "$T/kitty" '\x1b_Ga=p')" -gt 20 ] && echo yes)"
check "kitty: frames freed at exit" "$([ "$(count "$T/kitty" '\x1b_Ga=d,d=I')" = 6 ] && echo yes)"
check "kitty: every held frame is released" "$([ "$(count "$T/kitty" '\x1b[?2026h')" = "$(count "$T/kitty" '\x1b[?2026l')" ] && echo yes)"

python3 tests/ptyrun.py foot "$T/foot" >>"$T/log"
check "sixel: the wolf is drawn" "$([ "$(count "$T/foot" '\x1bP0;1;0q')" -gt 20 ] && echo yes)"
check "sixel: every held frame is released" "$([ "$(count "$T/foot" '\x1b[?2026h')" = "$(count "$T/foot" '\x1b[?2026l')" ] && echo yes)"

# a task done: the frost wolf (the first task wolf) runs, in its own coat, chasing the task's rune
python3 tests/ptyrun.py xterm-kitty "$T/kitty-task" "" task >>"$T/log"
check "kitty task: the frost wolf's six frames and one rune uploaded" "$([ "$(count "$T/kitty-task" '\x1b_Ga=t')" = 7 ] && [ "$(count "$T/kitty-task" 'i=25725,q=2,m=')" = 1 ] && [ "$(python3 -c "import re,sys; print(len(re.findall(rb'\x1b_Ga=t,[^;]*i=259(7[5-9]|8[0-9]|9[0-8]),', open(sys.argv[1],'rb').read())))" "$T/kitty-task")" = 1 ] && echo yes)"
check "kitty task: the frost wolf is placed" "$([ "$(count "$T/kitty-task" '\x1b_Ga=p,i=2572')" -gt 20 ] && echo yes)"
check "kitty task: its rune is placed" "$([ "$(python3 -c "import re,sys; print(len(re.findall(rb'\x1b_Ga=p,i=259(7[5-9]|8[0-9]|9[0-8]),', open(sys.argv[1],'rb').read())))" "$T/kitty-task")" -gt 20 ] && echo yes)"
check "kitty task: no snow wolf" "$([ "$(count "$T/kitty-task" 'i=25719,')" = 0 ] && echo yes)"
check "kitty task: the rune is taken down when it leaves" "$(python3 -c "
import re,sys
d=open(sys.argv[1],'rb').read()
ids=set(re.findall(rb'\x1b_Ga=p,i=(259(?:7[5-9]|8[0-9]|9[0-8])),', d))
print('yes' if ids and all(d.rfind(b'\x1b_Ga=d,d=i,i='+i) > d.rfind(b'\x1b_Ga=p,i='+i+b',') for i in ids) else 'no')" "$T/kitty-task")"
check "kitty task: images freed at exit" "$([ "$(count "$T/kitty-task" '\x1b_Ga=d,d=I')" = 7 ] && echo yes)"
python3 tests/ptyrun.py foot "$T/foot-task" "" task >>"$T/log"
check "sixel task: the wolf and rune are drawn" "$([ "$(count "$T/foot-task" '\x1bP0;1;0q')" -gt 40 ] && echo yes)"
check "sixel task: in frost blue" "$(grep -aq '#[0-9]*;2;[0-9]*;[0-9]*;[0-9]*' "$T/foot-task" && python3 -c "
import re,sys
d=open(sys.argv[1],'rb').read().decode('latin1')
c=set(re.findall(r'#\d+;2;(\d+);(\d+);(\d+)',d))
print('yes' if any(int(b)>int(r)+8 for r,g,b in c) else 'no')" "$T/foot-task")"

python3 tests/ptyrun.py xterm-256color "$T/plain" >>"$T/log"
check "plain terminal: no images" "$([ "$(count "$T/plain" '\x1b_G')" = 0 ] && [ "$(count "$T/plain" '\x1bP')" = 0 ] && echo yes)"
check "no sanitizer reports" "$(grep -q 'asan reports: none' "$T/log" && ! grep -q 'asan reports: \[' "$T/log" && echo yes)"
exit $fails
