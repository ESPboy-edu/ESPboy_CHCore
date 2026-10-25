#!/usr/bin/env python3
"""Build, test, package and upload the standalone CHGame soundtrack demo."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
ENV = dict(os.environ)
for key, relative in {
    'ARDUINO_CONFIG_FILE': 'arduino-cli.yaml',
    'ARDUINO_DIRECTORIES_DATA': '.arduino15',
    'ARDUINO_DIRECTORIES_DOWNLOADS': '.arduino15/staging',
    'ARDUINO_DIRECTORIES_USER': 'sketchbook',
    'ARDUINO_BUILD_CACHE_PATH': '.arduino15/cache',
}.items():
    ENV.setdefault(key, str(ROOT / relative))


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, env=ENV, check=True)


def main():
    args = sys.argv[1:] or ['build']
    python = ROOT / '.venv' / ('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    if args == ['setup']:
        if sys.version_info < (3, 12):
            raise SystemExit('Setup requires Python 3.12 or newer.')
        for tool in ('git', 'arduino-cli'):
            if not shutil.which(tool):
                raise SystemExit(f'Install {tool} and add it to PATH first.')
        run('git', 'submodule', 'update', '--init', '--recursive')
        if not python.exists():
            run(sys.executable, '-m', 'venv', ROOT / '.venv')
        run(python, '-m', 'pip', 'install', '-r', ROOT / 'requirements.txt')
        run('arduino-cli', 'core', 'update-index')
        run('arduino-cli', 'core', 'install', 'CHGame:ch32v@0.3.0')
        return 0
    tools = ROOT / 'CHGame/tools/chgame.py'
    if not tools.exists():
        raise SystemExit('Initialize the pinned dependency: git submodule update --init --recursive')
    interpreter = python if python.exists() else sys.executable
    if args == ['test']:
        run(interpreter, ROOT / 'Chiptune/tools/verify_audio.py')
        run(interpreter, ROOT / 'Chiptune/tools/verify_player.py')
        return 0
    return subprocess.call([str(interpreter), str(tools),
                            '--sketch', str(ROOT / 'Chiptune'), *args], cwd=ROOT, env=ENV)


if __name__ == '__main__':
    raise SystemExit(main())
