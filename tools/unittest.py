#!/usr/bin/env python3

import os
import sys
import glob
import subprocess
import multiprocessing
import math
import argparse


sys.path.insert(0,'local/bin/')

parser = argparse.ArgumentParser(
                    prog = 'ProgramName',
                    description = 'What the program does',
                    epilog = 'Text at the bottom of help')


parser.add_argument('-v', '--verbose',
                    action='store_true')  # on/off flag

args, remain = parser.parse_known_args()
print(args, remain)

paths = remain

if len(paths) == 0:
    paths = list(glob.glob('test/unit/*.c'))

def run_test(path):
    cfile = os.path.basename(path)
    name = os.path.splitext(cfile)[0]

    odir = f'artifacts/unit/{name}'
    os.system(f'mkdir -p {odir}')
    out = subprocess.run(f'local/bin/clang -O3 -emit-llvm -c -o {odir}/in.bc {path}', shell=True, capture_output=True)
    # print(out)
    if out.returncode != 0:
        return (path, False)
    os.system(f'cp {odir}/in.bc {odir}/out.bc')
    os.system(f'local/bin/llvm-dis {odir}/in.bc')

    out = subprocess.run(f'local/bin/alaska-transform {odir}/out.bc', shell=True, capture_output=True)
    if args.verbose:
        print(out.stderr.decode())
    if out.returncode == 0:
        os.system(f'local/bin/llvm-dis {odir}/out.bc')
    return (path, out.returncode == 0)



parallelism = None


with multiprocessing.Pool(parallelism) as pool:
    results = pool.imap(run_test, paths)
    fails = []
    for i, res in enumerate(results):
        path, success = res
        progress = f'{math.ceil(i/len(paths) * 100):3}%'
        if success:
            print(f'[\033[32mok\033[0m]: {path} {progress}')
        else:
            fails.append(path)
            print(f'[\033[31mfail\033[0m]: {path} {progress}')
            # print(f'{progress} \033[30;41m FAIL \033[0m {path}')
    print(f'{len(fails)} of {len(paths)} tests failed:')
    for fail in fails:
        print(fail)
    


