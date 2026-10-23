"""Keep the build user's home path out of embedded diagnostics and debug data."""
from pathlib import Path

Import('env')
env.Append(CCFLAGS=['-ffile-prefix-map=' + Path.home().as_posix() + '=/builder'])
