import waterline as wl
import waterline.suites
import waterline.utils
import waterline.pipeline
import shutil
import os
from pathlib import Path
import subprocess
from waterline.run import Runner
import pandas as pd
import time

import seaborn as sns
import matplotlib as mpl
from matplotlib.lines import Line2D
from matplotlib import cm
import matplotlib.pyplot as plt
import math

local = Path(os.path.realpath(os.path.dirname(__file__))).parent / "local"
os.environ["PATH"] = f"{local}/bin:{os.environ['PATH']}"
os.environ["LD_LIBRARY_PATH"] = f"{local}/lib:{os.environ['LD_LIBRARY_PATH']}"

space = wl.Workspace("bench")

enable_openmp = False


linker_flags = os.popen("alaska-config --ldflags").read().strip().split("\n")
# print("linker flags:", linker_flags)


class AlaskaLinker(wl.Linker):
    command = "clang++"
    args = [f"-L{local}/lib", "-lalaska"]

    def link(self, ws, objects, output, args=[]):
        # it's pretty safe to link using clang++.
        ws.shell("clang++", *args, '-ldl', *linker_flags, *objects, "-o", output)


class AlaskaStage(wl.pipeline.Stage):
    def run(self, input, output, benchmark):
        shutil.copy(input, output)
        print(f'alaska: compiling {input}')
        space.shell(f"{local}/bin/alaska-transform", output)
        space.shell("llvm-dis", output)

class AlaskaNoTrackingStage(wl.pipeline.Stage):
    def run(self, input, output, benchmark):
        shutil.copy(input, output)
        env = os.environ.copy()
        env['ALASKA_NO_TRACKING'] = 'true'
        space.shell(f"{local}/bin/alaska-transform", output, env=env)
        space.shell("llvm-dis", output)

class AlaskaNoHoistStage(wl.pipeline.Stage):
    def run(self, input, output, benchmark):
        shutil.copy(input, output)
        env = os.environ.copy()
        env['ALASKA_NO_HOIST'] = 'true'
        space.shell(f"{local}/bin/alaska-transform", output, env=env)
        space.shell("llvm-dis", output)


class AlaskaBaselineStage(wl.pipeline.Stage):
    def run(self, input, output, benchmark):
        shutil.copy(input, output)
        space.shell(f"{local}/bin/alaska-transform-baseline", output)
        space.shell("llvm-dis", output)


perf_stats = [
    "duration_time",

    "instructions",
    "branch-instructions",
    "branch-misses",
    "L1-dcache-loads",
    "L1-dcache-load-misses",
    "de_dis_dispatch_token_stalls1.store_queue_rsrc_stall", # We think this is store queue full
    "de_dis_dispatch_token_stalls1.load_queue_rsrc_stall", # We think this is load queue full

    "cycles",
]

run = 0
class PerfRunner(Runner):
    def run(self, workspace, config, binary):
        cwd = os.getcwd() if config.cwd is None else config.cwd
        print("running cd", cwd, "&&", binary, *config.args)
        global run
        run += 1
        with waterline.utils.cd(cwd):
            proc = subprocess.Popen(
                ['perf', 'stat', '-e', ','.join(perf_stats), '-x', ',', '-o', f'{binary}.{run}.perf.csv', binary, *[str(arg) for arg in config.args]],
                stdout=subprocess.DEVNULL,
                # stderr=subprocess.DEVNULL,
                env=config.env,
                cwd=cwd,
            )
            proc.wait()


        df = pd.read_csv(f'{binary}.{run}.perf.csv', comment='#', header=None)
        out = {}
        print(df)
        for val, name in zip(df[0], df[2]):
            # Sanity check the results...
            print(name, val)
            if val == '<not supported>':
                val = float("NAN")
            if val == '<not counted>':
                val = float("NAN")
            out[name] = int(val)
        # print(out)
        return out



all_spec = [
    600,
    602,
    605,
    620,
    623,
    625,
    631,
    641,
    657,
    619,
    638,
    644,
]

spec_enable = [
    605, # mcf
    623,
    625,
    631,
    641,
    657,
    619,
    638,
    644,
]

space.add_suite(wl.suites.Embench)
# space.add_suite(wl.suites.PolyBench, size="LARGE")
# space.add_suite(wl.suites.Stockfish)
space.add_suite(wl.suites.GAP, enable_openmp=enable_openmp, enable_exceptions=False, graph_size=20)
space.add_suite(wl.suites.NAS, enable_openmp=enable_openmp, suite_class="B")
space.add_suite(wl.suites.SPEC2017,
                tar="/home/nick/SPEC2017.tar.gz",
                disabled=[t for t in all_spec if t not in spec_enable],
                config="ref")

space.clear_pipelines()

pl = waterline.pipeline.Pipeline("alaska")
pl.add_stage(waterline.pipeline.OptStage(['-O3']), name="Optimize")
pl.add_stage(AlaskaStage(), name="Alaska")
pl.set_linker(AlaskaLinker())
space.add_pipeline(pl)


# pl = waterline.pipeline.Pipeline("alaska-nohoist")
# pl.add_stage(waterline.pipeline.OptStage(['-O3']), name="Optimize")
# pl.add_stage(AlaskaNoHoistStage(), name="Unhoisted-Alaska")
# pl.set_linker(AlaskaLinker())
# space.add_pipeline(pl)

pl = waterline.pipeline.Pipeline("alaska-untracked")
pl.add_stage(waterline.pipeline.OptStage(['-O3']), name="Optimize")
pl.add_stage(AlaskaNoTrackingStage(), name="Alaska-untracked")
pl.set_linker(AlaskaLinker())
space.add_pipeline(pl)

pl = waterline.pipeline.Pipeline("baseline")
pl.add_stage(waterline.pipeline.OptStage(['-O3']), name="Optimize")
pl.add_stage(AlaskaBaselineStage(), name="Baseline")
pl.set_linker(AlaskaLinker())
space.add_pipeline(pl)

results = space.run(runner=PerfRunner(), runs=5, compile=True)
print(results)
results.to_csv("bench/results.csv", index=False)
