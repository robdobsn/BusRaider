#!/usr/bin/env python3
import argparse
import logging
from pathlib import Path
import subprocess
from typing import Optional
import sys
import os

logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                    level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else Path(__file__).name)

CONFIG_DIRS_PATH = "buildConfigs"
REPO_ROOT = Path(__file__).parent.resolve().parent.resolve()
BUILD_CONFIGS_DIR = REPO_ROOT / CONFIG_DIRS_PATH


def parse_args():
    parser = argparse.ArgumentParser(
        description="Build/flash/monitor different RIC firmware configurations"
    )
    parser.add_argument('config_dir',
                        nargs="?",
                        type=Path,
                        help="Path to or name of a build config folder containing "
                             "revision-specific build options. "
                             "If not provided, all configs are built.")
    parser.add_argument('action',
                        nargs="?",
                        choices=['build', 'flash', 'monitor', 'app-flash'],
                        default='build',
                        help="Command for idf.py. Defaults to 'build'.")
    parser.add_argument('-p', '--port',
                        help="Serial port. (For flash/monitor.)")
    return parser.parse_args()


def get_all_config_dirs():
    unwanted_paths = ["",".",".."]
    return [cnf_dir for cnf_dir in BUILD_CONFIGS_DIR.glob("*")
            if os.path.isdir(cnf_dir)
            and cnf_dir not in unwanted_paths
            and not cnf_dir.name.startswith('.')]

def run_idf_for_config(config_dir: Path, action: str = 'build',
                       extra_idf_options: Optional[list] = None):
    _log.info("Processing config '%s'", config_dir)
    if extra_idf_options is None:
        extra_idf_options = []

    # Update sdkconfig if it is outdated
    config_dir_name = config_dir.name
    sdkconfig = config_dir / "sdkconfig"
    sdkconfig_defaults = config_dir / "sdkconfig.defaults"
    if sdkconfig.exists() and sdkconfig_defaults.stat().st_mtime > sdkconfig.stat().st_mtime:
        _log.info("Deleting outdated '%s'", sdkconfig)
        sdkconfig.unlink()

    # Run idf.py
    idf_options = ['-B', f'builds/{config_dir_name}'] + extra_idf_options
    idf_command = ['idf.py'] + idf_options + [action]
    _log.info("Executing '%s'...", " ".join(idf_command))
    subprocess.run(idf_command, check=True)
    _log.info("Done!\n")


def main():
    args = parse_args()
    _log.debug("args=%s", repr(args))

    if args.config_dir is None:
        for config_dir in get_all_config_dirs():
            run_idf_for_config(config_dir, 'build')
        return

    config_dir = args.config_dir
    if not config_dir.exists():
        config_dir = BUILD_CONFIGS_DIR / args.config_dir
        if not config_dir.exists():
            raise ValueError(f"Invalid config_dir: '{args.config_dir}' not found")

    if config_dir.parent.resolve() != BUILD_CONFIGS_DIR:
        raise ValueError(f"Invalid config_dir: '{args.config_dir}' is not a subdirectory "
                         f"of '{BUILD_CONFIGS_DIR}'")

    idf_options = []
    if args.port is not None:
        idf_options += ['-p', args.port]
    run_idf_for_config(config_dir, args.action, idf_options)


if __name__ == '__main__':
    main()
    sys.exit(0)
