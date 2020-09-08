#
# JSON Validator Utility for Firmware Development
#
# Rob Dobson 2020
#
import argparse
import re
import json
import sys

def extractJsonFromLine(line, jsonDiscovered):
    
    # Run along line handling chars
    inRString = False
    inString = False
    i = 0
    nonStringSemicolon = False
    while i < len(line):
        if inRString:
            if line[i] == ')' and line[i+1] == '"':
                inRString = False
                i += 2
            elif line[i] == '\\':
                jsonDiscovered += line[i+1]
                i += 2
            else:
                jsonDiscovered += line[i]
                i += 1
        elif inString:
            if line[i] == '"':
                inString = False
                i += 1
            elif line[i] == '\\':
                jsonDiscovered += line[i+1]
                i += 2
            else:
                jsonDiscovered += line[i]
                i += 1
        else:
            if line[i] == 'R' and line[i+1] == '"' and line[i+2] == '(':
                inRString = True
                i += 3
            elif line[i] == '"':
                inString = True
                i += 1
            elif line[i] == ';':
                nonStringSemicolon = True
                i += 1
            else:
                i += 1

    # Check incomplete string wrapping
    return nonStringSemicolon, inRString or inString, jsonDiscovered

    # # Lines with strings
    # firstInvComma = line.find('"')
    # lastInvComma = line.rfind('"')
    # stringEnd = False
    # if firstInvComma != lastInvComma:
    #     if line[firstInvComma-1] == 'R':
    #         jsonToAdd = line[firstInvComma+2:lastInvComma-1]
    #     else:
    #         jsonToAdd = line[firstInvComma+1:lastInvComma]
    #     jsonDiscovered += jsonToAdd
    #     stringEnd = line[lastInvComma+1:].find(";")
    # else:
    #     stringEnd = line.find(";")

    # Line contains ; not inside string
    # return stringEnd >= 0

def validateFile(fileName, varToCheck):
    
    jsonDiscovered = ""
    with open(fileName, 'r') as fileIn:
        # Read lines
        fileLines = fileIn.readlines()

        # Find line containing variable assignment - assume first reference in the file
        varFound = False
        lineIdx = 0
        for line in fileLines:
            lineIdx += 1
            if not varFound:
                if not varToCheck in line:
                    continue
                matchInfo = re.search(varToCheck + "\s*(\[\])?\s*=", line)
                if matchInfo:
                    varFound = True
                    isDone, isError, jsonDiscovered = extractJsonFromLine(line[matchInfo.end()+1:], jsonDiscovered)
                    if isError:
                        print("------------------------------------------------------------")
                        print("Unmatched inverted commas at line", lineIdx, "in file", fileName)
                        print("------------------------------------------------------------")
                        return False
                    if isDone:
                        break
                    continue
            else:
                isDone, isError, jsonDiscovered = extractJsonFromLine(line, jsonDiscovered)
                if isError:
                    print("------------------------------------------------------------")
                    print("Unmatched inverted commas at line", lineIdx, "in file", fileName)
                    print("------------------------------------------------------------")
                    return False
                if isDone:
                    break
        if not varFound:
            print("Var", varToCheck, "not found in source file", fileName)
            return False

    # Parse the JSON
    try:
        json.loads(jsonDiscovered)
    except Exception as excp:
        print("Failed to parse JSON in " + fileName + " error " + str(excp))
        print("------------------------------------------------------------")
        print("Suggest use https://jsonlint.com/ to validate the following:")
        print("------------------------------------------------------------")
        print(jsonDiscovered)
        print("------------------------------------------------------------")
        return False

    return True

if __name__ == '__main__':
    argparser = argparse.ArgumentParser(description='Embedded JSON Validator')
    argparser.add_argument('fileName', action='store')
    argparser.add_argument('varToCheck', action='store')
    args = argparser.parse_args()
    if validateFile(args.fileName, args.varToCheck):
        print(f"Validated JSON variable {args.varToCheck} in {args.fileName}")
        sys.exit(0)
    sys.exit(1)

