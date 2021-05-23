#!/usr/bin/env python3
import argparse
import json
import logging
import os
import re
import sys

logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else os.path.basename(__file__))

def parseArgs():
    parser = argparse.ArgumentParser(description="Convert JSON to C Header")
    parser.add_argument('inFile',
                        nargs='?',
                        type=argparse.FileType('r'),
                        default=sys.stdin,
                        help="JSON input file")
    parser.add_argument('outFile',
                        nargs='?',
                        type=argparse.FileType('w'),
                        default=sys.stdout,
                        help="C .h output file")
    return parser.parse_args()

def genHeaderFromJSON(inFile, outFile) -> None:
    # Generate header from JSON
    try:
        sys_types_json = json.load(inFile)
        for sys_type in sys_types_json:
            sys_type_lines = json.dumps(sys_type,separators=(',', ':'),indent="    ").splitlines()
            for line in sys_type_lines:
                indentGrp = re.search(r'^\s*', line)
                indentText = indentGrp.group(0) if indentGrp else ""
                outFile.write(f'{indentText}R"({line.strip()})"\n')
            outFile.write(f',\n')
    except ValueError as excp:
        _log.error(f"Failed JSON parsing of SysTypes JSON error {excp}")
        sys.exit(1)

def main():
    args = parseArgs()
    genHeaderFromJSON(args.inFile, args.outFile)

if __name__ == '__main__':
    main()
