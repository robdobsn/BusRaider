#!/usr/bin/env python3

from jinja2 import Environment, FileSystemLoader
import argparse
import logging
from pathlib import Path
import os
import gzip

logging.basicConfig(format="[%(asctime)s] %(levelname)s:%(name)s: %(message)s",
                    level=logging.INFO)
_log = logging.getLogger(__name__ if __name__ != '__main__' else Path(__file__).name)

def dir_path(string):
    if os.path.isdir(string):
        return string
    else:
        raise NotADirectoryError(string)

def parseArgs():
    parser = argparse.ArgumentParser(description="Generate Web UI using Jinja templates")
    parser.add_argument('source',
                        nargs='?',
                        type=dir_path,
                        help="folder containing input files")
    parser.add_argument('dest',
                        nargs='?',
                        type=dir_path,
                        help="folder to output files to")
    parser.add_argument('--nogzip', action='store_false', dest='gzipContent',
                        help='gzip the resulting files')
    return parser.parse_args()

def generateWebUI(sourceFolder, destFolder, gzipContent):

    _log.info("UITemplater source '%s' dest '%s' gzip %s", sourceFolder, destFolder, "Y" if gzipContent else "N")

    file_loader = FileSystemLoader(sourceFolder)
    env = Environment(loader=file_loader)

    for fname in os.listdir(sourceFolder):
        if fname.endswith('.jinja'):
            template = env.get_template('index.jinja')

            # output = template.render()
            # print(output)

            outname = os.path.splitext(os.path.basename(fname))[0] + ".html"
            outname = os.path.join(destFolder, outname)
            if gzipContent:
                renderedStr = template.render().encode('utf-8')
                with gzip.open(outname + '.gz', 'wb') as f:
                    f.write(renderedStr)
                _log.info(f"UITemplater JINJA created Web UI from {fname} to {outname}.gz")
            else:
                template.stream().dump(os.path.join(destFolder, outname))
                _log.info(f"UITemplater JINJA created Web UI from {fname} to {outname}")

def main():
    args = parseArgs()
    generateWebUI(args.source, args.dest, args.gzipContent)

if __name__ == '__main__':
    main()
