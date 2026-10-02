# Run dew in a pty that reports pixel sizes; script keys; save everything it writes.
import os, pty, sys, time, fcntl, termios, struct, select, tempfile, shutil
term, out = sys.argv[1], sys.argv[2]
extra = sys.argv[3] if len(sys.argv) > 3 else ''
cols, rows, cw, ch = 110, 34, 10, 20
T = tempfile.mkdtemp()
os.makedirs(T + '/cfg/dew'); open(T + '/cfg/dew/config', 'w').write('notify = 0\nbell = 0\n' + extra)
env = dict(os.environ, TERM=term, XDG_DATA_HOME=T + '/data', XDG_CONFIG_HOME=T + '/cfg',
           ASAN_OPTIONS='log_path=' + T + '/asan')
for k in ('KITTY_WINDOW_ID', 'TMUX', 'TERM_PROGRAM'): env.pop(k, None)
os.system(f'env XDG_DATA_HOME={T}/data ./dew add -t one >/dev/null')
pid, fd = pty.fork()
if pid == 0:
    fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, cols * cw, rows * ch))
    os.execve('./dew', ['dew'], env)
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, cols * cw, rows * ch))
buf = bytearray()
def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try: buf.extend(os.read(fd, 65536))
            except OSError: return
for key, wait in [(b'', 1.0), (b' ', 1.5), (b's', 6.0), (b'?', 0.6), (b'?', 0.6), (b'q', 1.0)]:
    if key: os.write(fd, key)
    pump(wait)
os.waitpid(pid, 0)
open(out, 'wb').write(bytes(buf))
asan = [f for f in os.listdir(T) if f.startswith('asan')]
print('bytes', len(buf), 'asan reports:', asan or 'none')
for f in asan: print(open(T + '/' + f).read()[:2000])
shutil.rmtree(T)
