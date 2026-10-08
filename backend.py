"""Unprivileged build and lock monitoring; never receives keyboard events."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent


def build():
    source = ROOT / 'reader.c'
    target = ROOT / 'falling-keys-reader'
    if target.is_file() and target.stat().st_mtime_ns >= source.stat().st_mtime_ns:
        return
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'xkbcommon'], text=True).split()
    with tempfile.TemporaryDirectory(prefix='.build-', dir=ROOT) as temporary:
        output = Path(temporary) / 'reader'
        subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                        '-fstack-protector-strong', '-D_FORTIFY_SOURCE=2',
                        str(source), '-o', str(output), *flags], check=True)
        output.chmod(0o755)
        output.replace(target)


def lock_state(monitors):
    if not isinstance(monitors, list) or not monitors:
        return 'unknown'
    blockers = [([] if m.get('solitaryBlockedBy') is None and m.get('solitary') not in (None, '', '0')
                 else m.get('solitaryBlockedBy')) for m in monitors if isinstance(m, dict)]
    if len(blockers) != len(monitors) or any(not isinstance(b, list) for b in blockers):
        return 'unknown'
    if any('LOCK' in b for b in blockers):
        return 'locked'
    return 'unlocked' if any('WORKSPACE' not in b for b in blockers) else 'unknown'


def watch():
    path = Path(os.environ['XDG_RUNTIME_DIR']) / 'hypr' / os.environ['HYPRLAND_INSTANCE_SIGNATURE'] / '.socket.sock'
    first = True
    while True:
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                connection.settimeout(1)
                connection.connect(str(path))
                connection.sendall(b'j/monitors')
                data = bytearray()
                while True:
                    chunk = connection.recv(65536)
                    if not chunk:
                        break
                    data.extend(chunk)
                    if len(data) > 1024 * 1024:
                        raise ValueError('Oversized compositor response')
            state = lock_state(json.loads(data))
        except (OSError, ValueError):
            state = 'unknown'
        if first or state != 'unlocked':
            print(state, flush=True)
        if state != 'unlocked':
            return
        first = False
        time.sleep(0.5)


if __name__ == '__main__':
    try:
        if sys.argv[1:] == ['build']:
            build()
        elif sys.argv[1:] == ['watch']:
            watch()
        else:
            raise ValueError('Usage: python3 backend.py build|watch')
    except (OSError, KeyError, ValueError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
