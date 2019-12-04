# Copyright 2019 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

import argparse
import enum
import glob
import logging
import os
import pathlib
import subprocess
import sys
import time

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer
from watchdog.utils import has_attribute
from watchdog.utils import unicode_paths

import pw_cli.plugins
from pw_cli.color import Color as _Color

import logging
_LOG = logging.getLogger(__name__)

_PASS_MESSAGE = """
  ██████╗  █████╗ ███████╗███████╗██╗
  ██╔══██╗██╔══██╗██╔════╝██╔════╝██║
  ██████╔╝███████║███████╗███████╗██║
  ██╔═══╝ ██╔══██║╚════██║╚════██║╚═╝
  ██║     ██║  ██║███████║███████║██╗
  ╚═╝     ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝
"""

# Pick a visually-distinct font from "PASS" to ensure that readers can't
# possibly mistake the difference between the two states.
_FAIL_MESSAGE = """
   ▄██████▒░▄▄▄       ██▓  ░██▓
  ▓█▓     ░▒████▄    ▓██▒  ░▓██▒
  ▒████▒   ░▒█▀  ▀█▄  ▒██▒ ▒██░
  ░▓█▒    ░░██▄▄▄▄██ ░██░  ▒██░
  ░▒█░      ▓█   ▓██▒░██░░ ████████▒
   ▒█░      ▒▒   ▓▒█░░▓  ░  ▒░▓  ░
   ░▒        ▒   ▒▒ ░ ▒ ░░  ░ ▒  ░
   ░ ░       ░   ▒    ▒ ░   ░ ░
                 ░  ░ ░       ░  ░
"""


class _State(enum.Enum):
    WAITING_FOR_FILE_CHANGE_EVENT = 1
    COOLDOWN_IGNORING_EVENTS = 2


# TODO(keir): Figure out a better strategy for exiting. The problem with the
# watcher is that doing a "clean exit" is slow. However, by directly exiting,
# we remove the possibility of the wrapper script from doing anything on exit.
def _die(*args):
    _LOG.fatal(*args)
    sys.exit(1)


class PigweedBuildWatcher(FileSystemEventHandler):
    def __init__(self,
                 patterns=None,
                 ignore_patterns=None,
                 case_sensitive=False,
                 build_dirs=[]):
        super().__init__()

        self.patterns = patterns
        self.ignore_patterns = ignore_patterns
        self.case_sensitive = case_sensitive
        self.state = _State.WAITING_FOR_FILE_CHANGE_EVENT
        self.build_dirs = build_dirs

    def path_matches(self, path):
        """Returns true if path matches according to the watcher patterns"""
        pure_path = pathlib.PurePath(path)
        return ((not any(pure_path.match(x) for x in self.ignore_patterns))
                and any(pure_path.match(x) for x in self.patterns))

    def dispatch(self, event):
        # There isn't any point in triggering builds on new directory creation.
        # It's the creation or modification of files that indicate something
        # meaningful enough changed for a build.
        if event.is_directory:
            return

        # Collect paths of interest from the event.
        paths = []
        if has_attribute(event, 'dest_path'):
            paths.append(unicode_paths.decode(event.dest_path))
        if event.src_path:
            paths.append(unicode_paths.decode(event.src_path))
        for path in paths:
            _LOG.debug('File event: %s', path)

        for path in paths:
            if self.path_matches(path):
                _LOG.debug('Match for path: %s', path)
                self.on_any_event()

    def run_builds(self):
        # Run all the builds in serial and capture pass/fail for each.
        builds_succeeded = []
        num_builds = len(self.build_dirs)
        _LOG.info(f'Starting build with {num_builds} directories')
        for i, build_dir in enumerate(self.build_dirs, 1):
            _LOG.info(f'[{i}/{num_builds}] Starting build: {build_dir}')

            # Run the build. Put a blank before/after for visual separation.
            print()
            result = subprocess.run(['ninja', '-C', build_dir])
            print()

            build_ok = (result.returncode == 0)
            if build_ok:
                tag = '(OK)'
            else:
                tag = '(FAIL)'
            _LOG.info(f'[{i}/{num_builds}] Finished build: {build_dir} {tag}')
            builds_succeeded.append(build_ok)

        if all(builds_succeeded):
            _LOG.info('Finished; all successful.')
        else:
            _LOG.info('Finished; some builds failed.')

        # Write out build summary table so you can tell which builds passed
        # and which builds failed.
        print()
        print(' .------------------------------------')
        print(' |')
        for (succeeded, build_dir) in zip(builds_succeeded, self.build_dirs):
            if succeeded:
                slug = _Color.green('OK  ')
            else:
                slug = _Color.red('FAIL')

            print(f' |   {slug}  {build_dir}')
        print(' |')
        print(" '------------------------------------")

        # Show a large color banner so it is obvious what the overall result is.
        if all(builds_succeeded):
            print(_Color.green(_PASS_MESSAGE))
        else:
            print(_Color.red(_FAIL_MESSAGE))

    def on_any_event(self):
        if self.state == _State.WAITING_FOR_FILE_CHANGE_EVENT:
            self.run_builds()

            # Don't set the cooldown end time until after the build.
            self.state = _State.COOLDOWN_IGNORING_EVENTS
            _LOG.debug('State: WAITING -> COOLDOWN (file change trigger)')

            # 500ms is enough to allow the spurious events to get ignored.
            self.cooldown_finish_time = time.time() + 0.5

        elif self.state == _State.COOLDOWN_IGNORING_EVENTS:
            if time.time() < self.cooldown_finish_time:
                _LOG.debug('Skipping event; cooling down...')
            else:
                _LOG.debug('State: COOLDOWN -> WAITING (cooldown expired)')
                self.state = _State.WAITING_FOR_FILE_CHANGE_EVENT
                self.on_any_event()  # Retrigger.


_WATCH_PATTERN_DELIMITER = ','
_WATCH_PATTERNS = (
    '*.bloaty',
    '*.c',
    '*.cc',
    '*.gn',
    '*.gni',
    '*.h',
    '*.ld',
    '*.py',
    '*.rst',
)


def add_parser_arguments(parser):
    parser.add_argument('--patterns',
                        help=(_WATCH_PATTERN_DELIMITER +
                              '-delimited list of globs to '
                              'watch to trigger recompile'),
                        default=_WATCH_PATTERN_DELIMITER.join(_WATCH_PATTERNS))
    parser.add_argument('--ignore_patterns',
                        help=(_WATCH_PATTERN_DELIMITER +
                              '-delimited list of globs to '
                              'ignore events from'))
    parser.add_argument(
        '--build_dir',
        help=('Ninja directory to build. Can be specified '
              'multiple times to build multiple configurations'),
        action='append')


def watch(build_dir='', patterns=None, ignore_patterns=None):
    _LOG.info('Starting Pigweed build watcher')

    # If no build directory was specified, search the tree for GN build
    # directories and try to build them all. In the future this may cause
    # slow startup, but for now this is fast enough.
    build_dirs = build_dir
    if not build_dirs:
        build_dirs = []
        _LOG.info('Searching for GN build dirs...')
        gn_args_files = glob.glob('**/args.gn', recursive=True)
        for gn_args_file in gn_args_files:
            gn_build_dir = pathlib.Path(gn_args_file).parent
            if gn_build_dir.is_dir():
                build_dirs.append(str(gn_build_dir))

    # Make sure we found something; if not, bail.
    if not build_dirs:
        _die("No build dirs found. Did you forget to 'gn gen out'?")

    # Verify that the build output directories exist.
    for i, build_dir in enumerate(build_dirs, 1):
        if not os.path.isdir(build_dir):
            _die("Build directory doesn't exist: %s", build_dir)
        else:
            _LOG.info(f'Will build [{i}/{len(build_dirs)}]: {build_dir}')

    _LOG.debug('Patterns: %s', patterns)

    # TODO(keir): Change the watcher to selectively watch some
    # subdirectories, rather than watching everything under a single path.
    #
    # The problem with the current approach is that Ninja's building
    # triggers many events, which are needlessly sent to this script.
    path_of_directory_to_watch = '.'

    # Try to make a short display path for the watched directory that has
    # "$HOME" instead of the full home directory. This is nice for users
    # who have deeply nested home directories.
    path_to_log = pathlib.Path(path_of_directory_to_watch).resolve()
    try:
        path_to_log = path_to_log.relative_to(pathlib.Path.home())
        path_to_log = f'$HOME/{path_to_log}'
    except ValueError:
        # The directory is somewhere other than inside the users home.
        path_to_log = path_of_directory_to_watch

    # We need to ignore both the user-specified patterns and also all
    # events for files in the build output directories.
    ignore_patterns = (ignore_patterns.split(_WATCH_PATTERN_DELIMITER)
                       if ignore_patterns else [])
    ignore_patterns.extend([f'{build_dir}/*' for build_dir in build_dirs])

    event_handler = PigweedBuildWatcher(
        patterns=patterns.split(_WATCH_PATTERN_DELIMITER),
        ignore_patterns=ignore_patterns,
        build_dirs=build_dirs)

    observer = Observer()
    observer.schedule(
        event_handler,
        path_of_directory_to_watch,
        recursive=True,
    )
    observer.start()

    _LOG.info('Directory to watch: %s', path_to_log)
    _LOG.info('Watching for file changes. Ctrl-C exits.')

    # Make a nice non-logging banner to motivate the user.
    print()
    print(_Color.green('  WATCHER IS READY: GO WRITE SOME CODE!'))
    print()

    try:
        while observer.isAlive():
            observer.join(1)
    except KeyboardInterrupt:
        # To keep the log lines aligned with each other in the presence of
        # a '^C' from the keyboard interrupt, add a newline before the log.
        print()
        _LOG.info('Got Ctrl-C; exiting...')

        # Note: The "proper" way to exit is via observer.stop(), then
        # running a join. However it's slower, so just exit immediately.
        sys.exit(0)

    observer.join()


pw_cli.plugins.register(
    name='watch',
    help='Watch files for changes',
    define_args_function=add_parser_arguments,
    command_function=watch,
)


def main():
    parser = argparse.ArgumentParser(description='Watch for changes')
    add_parser_arguments(parser)
    args = parser.parse_args()
    watch(**vars(args))


if __name__ == '__main__':
    main()
