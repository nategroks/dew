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

python3 tests/ptyrun.py xterm-256color "$T/plain" >>"$T/log"
check "plain terminal: no images" "$([ "$(count "$T/plain" '\x1b_G')" = 0 ] && [ "$(count "$T/plain" '\x1bP')" = 0 ] && echo yes)"
check "no sanitizer reports" "$(grep -q 'asan reports: none' "$T/log" && ! grep -q 'asan reports: \[' "$T/log" && echo yes)"
exit $fails
