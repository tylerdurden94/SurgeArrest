"""Private setup data belongs in the user's application-data folder."""
import os
from pathlib import Path


def private_dir():
    if os.name == 'nt':
        return Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local')) / 'SurgeArrest'
    if __import__('sys').platform == 'darwin':
        return Path.home() / 'Library/Application Support/SurgeArrest'
    return Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'SurgeArrest'


def write_private(path, text):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if os.name != 'nt':
        path.parent.chmod(0o700)
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
        stream.write(text)
