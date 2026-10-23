"""Compile and run the actual production delivery state machine on the host."""
import argparse
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig', type=Path)
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='surgearrest-test-') as directory:
        executable = Path(directory) / 'test_delivery.exe'
        compiler = [str(args.zig), 'c++'] if args.zig else [args.cxx]
        subprocess.run(compiler + ['-std=c++17', '-Wall', '-Wextra', '-Werror',
            '-I' + str(here / 'include'), str(here / 'tests/test_delivery.cpp'), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    main()
