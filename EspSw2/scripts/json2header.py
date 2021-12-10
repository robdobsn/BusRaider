#!/usr/bin/env python3

"""This script checks the validity of a JSON file and converts it to a C++ header.

The checks include validation of any custom extensions to JSON (e.g. revision
switch arrays). Invalid input files are not converted into C++ headers and should
trigger a build failure.

This is currently used for SysTypes.json.
"""

import argparse
import json
import logging
import os
import re
import sys
from typing import Any, Dict, List, Set, Tuple, Union


logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else os.path.basename(__file__))


OPTION_OBJ_KEYS = {"__hwRevs__", "__value__"}


def validateRevisionSwitchArrays(jsonElement: Union[list, dict]) -> bool:
    """
    Validate the given JSON tree with regard to our custom syntax extension for
    revision switching.

    This function recursively traverses the JSON tree looking for revision switch
    arrays. When one is found, it's format is checked. The checks include:
        - The array cannot contain a mix of option objects and other (normal) elements.
        - There can be at most one default option object.
        - The option objects cannot have overlapping __hwRevs__ specifications.

    Further, each option object must match the following to be considered valid:
        - It must contain exactly two keys and these must be "__hwRevs__" and "__value__".
        - "__hwRevs__" must be an array of unique integers.

    It is also checked that "__hwRevs__" and "__value__" are not used anywhere
    other than option objects.

    The validation is intentionally more restrictive than the parsing in ConfigBase.cpp.
    """

    isValid, pathToError = True, ""
    if isinstance(jsonElement, dict):
            isValid, pathToError = _validateObject(jsonElement)
    elif isinstance(jsonElement, list):
        isValid, pathToError = _validateArray(jsonElement)
    else:
        raise ValueError("validateRevisionSwitchArrays() accepts lists and dicts"
                         f" but got {type(jsonElement)}")
    if not isValid:
        _log.error(f"Invalid JSON element found at '{pathToError}'")
    return isValid


def _validateObject(jsonObj: Dict[str, Any]) -> Tuple[bool, str]:
    """Validate a normal JSON object (i.e. not an option object)

    Returns:
        bool: Whether or not `jsonObj` is valid
        str:  Path (XPath-ish) to an error if `jsonObj` is invalid, `""` otherwise
    """

    for k, v in jsonObj.items():
        if k in OPTION_OBJ_KEYS:
            _log.error(f"Reserved key '{k}' found outside an option object.")
            return False, ""

        isValid, pathToError = True, ""
        if isinstance(v, dict):
            isValid, pathToError = _validateObject(v)
        elif isinstance(v, list):
            isValid, pathToError = _validateArray(v)
        if not isValid:
            return False, f"{k}/{pathToError}"

    return True, ""


def _validateArray(jsonArray: list) -> Tuple[bool, str]:
    """Validate any JSON array

    Returns:
        bool: Whether or not `jsonArray` is valid
        str:  Path (XPath-ish) to an error if `jsonArray` is invalid, `""` otherwise
    """

    hasNormalVals, hasOptionObjs = False, False
    hwRevSets = []
    for i, elem in enumerate(jsonArray):
        isValid, pathToError = True, ""
        if isinstance(elem, list):
            hasNormalVals = True
            isValid, pathToError = _validateArray(elem)
        elif isinstance(elem, dict):
            if _isOptionObj(elem):
                hasOptionObjs = True
                hwRevSets.append(set(elem["__hwRevs__"]))
            elif _isNormalObj(elem):
                hasNormalVals = True
                isValid, pathToError = _validateObject(elem)
            else:
                # Corrupted object
                return False, f"[{i}]"
        else:
            # Primitive value (string/number/boolean/null)
            hasNormalVals = True
        if not isValid:
            return False, f"[{i}]/{pathToError}"
    if hasNormalVals and hasOptionObjs:
        _log.error(f"Mixed array of option objects and normal JSON elements found.")
        return False, ""
    if not _validateHwRevSets(hwRevSets):
        return False, ""
    return True, ""


def _isOptionObj(jsonObj: Dict[str, Any]) -> bool:
    logPrefix = "Option object corrupted: "
    currObjKeys = set(jsonObj.keys())
    if currObjKeys != OPTION_OBJ_KEYS:
        extraKeys = currObjKeys - OPTION_OBJ_KEYS
        missingKeys = OPTION_OBJ_KEYS - currObjKeys
        if missingKeys != OPTION_OBJ_KEYS:
            if len(extraKeys) > 0:
                _log.error(f"{logPrefix}keys {extraKeys} not recognised.")
            if len(missingKeys) > 0:
                _log.error(f"{logPrefix}required keys {missingKeys} not found.")
        return False

    hwRevs = jsonObj["__hwRevs__"]
    if not isinstance(hwRevs, list):
        _log.error(f"{logPrefix}__hwRevs__ must be an array but was {type(hwRevs)}")
        return False
    if not all(map(lambda revNum: type(revNum) is int, hwRevs)):
        _log.error(f"{logPrefix}__hwRevs__ must be an array of ints but contained {set(map(type, hwRevs))}")
        return False
    if len(hwRevs) != len(set(hwRevs)):
        _log.error(f"{logPrefix}__hwRevs__ has duplicate elements ({sorted(hwRevs)})")
        return False
    return True


def _isNormalObj(jsonObj: Dict[str, Any]) -> bool:
    return set(jsonObj.keys()).isdisjoint(OPTION_OBJ_KEYS)


def _validateHwRevSets(hwRevSets: List[Set[int]]) -> bool:
    """
    Check that __hwRevs__ specifications from multiple option objects do not overlap.
    """

    for i in range(len(hwRevSets) - 1):
        for j in range(i + 1, len(hwRevSets)):
            if len(hwRevSets[i]) == 0 == len(hwRevSets[j]):
                _log.error(f"Two default option objects found at positions {i} and {j}")
                return False
            if not hwRevSets[i].isdisjoint(hwRevSets[j]):
                _log.error(f"Option objects {i} and {j} have overlapping __hwRevs__ "
                           + str(hwRevSets[i] & hwRevSets[j]))
                return False
    return True


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
        if not validateRevisionSwitchArrays(sys_types_json):
            _log.error(f"JSON revision switching error(s) found in {str(inFile.name)}")
            sys.exit(2)
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
