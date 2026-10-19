"""The network game's server for the tests: the Go program's (build/openmtv serve,
launcher/mpserver.go; $MADTV_GO_EXE: another build of it, e.g. the race detector's).

    netserver.command() + ['--seats', '2', ...]

Standard library only."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
GO = os.path.join(ROOT, 'build', 'openmtv' + ('.exe' if os.name == 'nt' else ''))


def command():
    """the server's command line (without its options)"""
    exe = os.environ.get('MADTV_GO_EXE') or GO
    if not os.path.exists(exe):
        sys.exit('%s is not built (make launcher)' % os.path.relpath(exe, ROOT))
    return [exe, 'serve']
