#!/usr/bin/env python3

import os
import sys
import glob
import subprocess
import multiprocessing
import math
import argparse
import time



sys.path.insert(0,'local/bin/')

parser = argparse.ArgumentParser(
                    prog = 'ProgramName',
                    description = 'What the program does',
                    epilog = 'Text at the bottom of help')


parser.add_argument('-v', '--verbose',
                    action='store_true')  # on/off flag


parser.add_argument('-s', '--slowest',
                    action='store_true')  # on/off flag
args, remain = parser.parse_known_args()

parallelism = 1

paths = remain

if len(paths) == 0:
    parallelism = None # use all the cores, counterintuitively
    paths = list(glob.glob('test/unit/*.c'))

def run_test(path):
    start = time.time_ns();
    cfile = os.path.basename(path)
    name = os.path.splitext(cfile)[0]

    odir = f'artifacts/unit/{name}'
    os.system(f'mkdir -p {odir}')
    out = subprocess.run(f'local/bin/clang -O0 -emit-llvm -c -o {odir}/in.bc {path}', shell=True, capture_output=True)
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
    return (path, out.returncode == 0, time.time_ns() - start)





def progress_bar(current, total):
    p = current / float(total)
    l = 20
    hashes = math.ceil(p * l)
    return f'{int(p * 100):3}% [\033[32m' + ('|' * hashes) + (' ' * (l - hashes)) + '\033[0m]'

try:
    with multiprocessing.Pool(parallelism) as pool:
        results = pool.imap_unordered(run_test, paths)

        fails = 0
        times = []
        for i, res in enumerate(results):
            path, success, time = res
            time = int(time / 1000.0 / 1000.0)
            progress = f'({i:4}/{len(paths):4})'
            progress = progress_bar(i, len(paths))
            times.append((path, time))
            if success:
                print(f'\033[2K{progress} {path}', end='\n' if args.verbose else '\r')
            else:
                fails += 1
                print(f'\033[2K\033[31mFAIL\033[0m: {path}')
                # print(f'{progress} \033[30;41m FAIL \033[0m {path}')
        

        
        # clear the last line
        print('\033[2K', end='\r')
        if fails > 0:
            print(f'Failed tests: {fails}')
        else:
            print('Success!')
        # cleanup the terminal output
        # print('\033[2K', end='\r' if args.slowest else '\n')
        if args.slowest:
            times.sort(reverse=True, key=lambda a: a[1])
            print("Slowest 10 tests:")
            for test, time in times[0:10]:
                print(f'{time:5}ms: {test}')
except:
    pass
