"""Build and install into this user's Omarchy configuration; never run as root."""
import os
from pathlib import Path
import shutil
import tempfile
from backend import ROOT, build

FILES = ('manifest.json', 'BarWidget.qml', 'Service.qml', 'Rain.qml', 'Layouts.js',
         'PercentControl.qml', 'reader.c', 'backend.py', 'falling-keys-reader',
         'README.md', 'LICENSE', 'SECURITY.md', 'install.py')


def main():
    if os.geteuid() == 0:
        raise SystemExit('Run as your normal desktop user.')
    build()
    target = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'omarchy/plugins/io.github.dfrost90.falling-keys'
    if ROOT != target.resolve():
        target.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.install-', dir=target.parent) as temporary:
            staging = Path(temporary)
            for name in FILES:
                shutil.copy2(ROOT / name, staging / name)
            for name in FILES:
                (staging / name).replace(target / name)
    print(f'Installed Key Rain in {target}')


if __name__ == '__main__':
    main()
