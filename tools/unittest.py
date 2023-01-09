#!/usr/bin/env python3

import os
import sys
import glob
import subprocess
import multiprocessing
import argparse
import time



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



parallelism = None


with multiprocessing.Pool(parallelism) as pool:
    results = pool.imap_unordered(run_test, paths)
    fails = []

    times = []
    for i, res in enumerate(results):
        path, success, time = res
        time = int(time / 1000.0 / 1000.0)
        progress = f'({i:4}/{len(paths):4})'
        times.append((path, time))
        if success:
            print(f'\033[2K[\033[32mok\033[0m]: {progress} {time:5}ms {path}', end='\r')
        else:
            fails.append(path)
            print(f'\033[2K[\033[31m!!\033[0m]: {progress} {time:5}ms {path}')
            # print(f'{progress} \033[30;41m FAIL \033[0m {path}')
    
    times.sort(reverse=True, key=lambda a: a[1])

    print()
    print("Slowest 10 tests:")
    for test, time in times[0:10]:
        print(f'{time:5}ms: {test}')

    print(f'{len(fails)} of {len(paths)} tests failed:')
    for fail in fails:
        print(fail)
    


