#!/usr/bin/env python3

import os
import sys
import glob
import subprocess
import multiprocessing
import math


paths = sys.argv[1:]

if len(paths) == 0:
    paths = list(glob.glob('test/unit/*.c'))

def run_test(path):
    cfile = os.path.basename(path)
    name = os.path.splitext(cfile)[0]

    odir = f'artifacts/unit/{name}'
    os.system(f'mkdir -p {odir}')
    out = subprocess.run(f'local/bin/clang -O3 -emit-llvm -c -o {odir}/in.ll {path}', shell=True, capture_output=True)
    if out.returncode != 0:
        return (path, False)
    os.system(f'cp {odir}/in.ll {odir}/out.ll')

    out = subprocess.run(f'local/bin/alaska-transform {odir}/out.ll', shell=True, capture_output=True)
    return (path, out.returncode == 0)




parallelism = None


with multiprocessing.Pool(parallelism) as pool:
    results = pool.imap(run_test, paths)
    fails = []
    for i, res in enumerate(results):
        path, success = res
        # progress = f'[{i:4}/{len(paths):4}]'
        progress = f'{math.ceil(i/len(paths) * 100):3}%'
        if success:
            print(f'{progress} \033[30;42m PASS \033[0m {path}')
        else:
            fails.append(path)
            print(f'{progress} \033[30;41m FAIL \033[0m {path}')
    print(f'{len(fails)} of {len(paths)} tests failed:')
    for fail in fails:
        print(fail)
    


