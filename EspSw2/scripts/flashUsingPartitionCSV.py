#!/usr/bin/env python3
import argparse
import csv
import logging
from pathlib import Path
import subprocess
import sys
from typing import Optional

logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                    level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else Path(__file__).name)

CONFIG_DIRS_PATH = "buildConfigs"
REPO_ROOT = Path(__file__).parent.resolve()
BUILD_CONFIGS_DIR = REPO_ROOT / CONFIG_DIRS_PATH


def parse_args():
    parser = argparse.ArgumentParser(
        description="Flash firmware elements using partitions.csv"
    )
    parser.add_argument('partitions_csv',
                        type=argparse.FileType('r'),
                        help="Full path and name of partitions csv file")
    parser.add_argument('build_folder',
                        type=Path,
                        help="Path to folder containing binaries to flash")
    parser.add_argument('firmware_binary_name',
                        help="Name of the firmware binary (excluding path)")
    parser.add_argument('port',
                        help="Serial port")
    parser.add_argument('-b', '--baud', default='2000000',
                        help="Baud rate for serial port")
    parser.add_argument('-f', '--filesysimage', default='',
                        help="Name of the file system image (excluding path)")
    return parser.parse_args()

def parse_partitions_csv(partitions_csv_file):
    partitions = {}
    reader = csv.reader(partitions_csv_file, delimiter=',')
    for row in reader:
        if (len(row) < 5 or ('#' in row[0])):
            continue
        partitions[row[0].strip()] = {
            "type": row[1].strip(),
            "subType": row[2].strip(),
            "offset": row[3].strip(),
            "size": row[4].strip()
        }
    return partitions

def convert_arg_str(args, strToConv):
    if strToConv[0] == '$':
        # print(strToConv[1:] in args)
        return vars(args)[strToConv[1:]]
    return strToConv

def convert_offset_str(partitionTable, strToConv):
    if strToConv[0] == '$':
        # print(strToConv[1:] in args)
        return partitionTable[strToConv[1:]].get("offset", "")
    return strToConv

def main():
    args = parse_args()
    _log.debug("args=%s", repr(args))

    # Check build folder valid
    build_folder = args.build_folder
    if not build_folder.exists():
        raise ValueError(f"Invalid build_folder: '{args.build_folder}' not found")

    # Parse partitions file
    partitions = parse_partitions_csv(args.partitions_csv)
    print(partitions)

    # Flash files and offsets
    filesToFlash = [
        ["bootloader/bootloader.bin","0x1000"],
        ["partition_table/partition-table.bin", "0x8000"],
        ["ota_data_initial.bin", "$otadata"],
        ["$firmware_binary_name", "$app0"],
        ["$filesysimage", "$spiffs"]
    ]

    # Run esptool using partition info
    esptool_options = ['-p', f'{args.port}']
    if args.baud is not None:
        esptool_options += ['-b', args.baud]
    esptool_options += ['--before', "default_reset"]
    esptool_options += ['--after', "hard_reset"]
    esptool_options += ['--chip', "esp32"]
    esptool_options += ['write_flash']
    esptool_options += ['--flash_mode', "dio"]
    esptool_options += ['--flash_size', "detect"]
    esptool_options += ['--flash_freq', "40m"]

    # Check build folder contains all the files to flash 
    # and partitions table likewise for partitions to flash into
    for fileToFlash in filesToFlash:
        flashFileName = convert_arg_str(args, fileToFlash[0])
        if len(flashFileName) == 0:
            continue
        if not (build_folder / flashFileName).is_file():
            raise ValueError(f"File {flashFileName} not found in build folder {args.build_folder}")
        flashOffset = convert_offset_str(partitions, fileToFlash[1])
        if len(flashOffset) == 0:
            raise ValueError(f"Partition {fileToFlash[1]} not found in partition table")
        esptool_options += [flashOffset, str(args.build_folder / flashFileName)]

    # Form command
    esptoolPossNames = ['esptool.py.exe','esptool','esptool.py']
    espToolCmdFound = False
    lastExcp = None
    for espToolName in esptoolPossNames:
        esptool_command = [espToolName] + esptool_options
        _log.info("Executing '%s'...", " ".join(esptool_command))
        rslt = None
        try:
            rslt = subprocess.run(esptool_command, check=True)
            espToolCmdFound = True
            break
        except Exception as excp:
            lastExcp = excp
            rslt = None
            continue
    if not espToolCmdFound and lastExcp is not None:
        _log.error(f"Failed to execute esptool command {str(lastExcp)}")
    if rslt is not None and rslt.returncode == 0:
        _log.info("Done!\n")
    else:
        _log.error(f"esptool command returned non-zero code {rslt}")
        return -1 if rslt is None else rslt.returncode
    return 0

if __name__ == '__main__':
    rslt = main()
    sys.exit(rslt)
