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
print("linker flags:", linker_flags)


class AlaskaLinker(wl.Linker):
    command = "clang++"
    args = [f"-L{local}/lib", "-lalaska"]

    def link(self, ws, objects, output, args=[]):
        # it's pretty safe to link using clang++.
        ws.shell("clang++", *args, '-ldl', *linker_flags, *objects, "-o", output)


class AlaskaStage(wl.pipeline.Stage):
    def run(self, input, output, benchmark):
        shutil.copy(input, output)
        space.shell(f"{local}/bin/alaska-transform", output)
        space.shell("llvm-dis", output)


class AlaskaBaselineStage(wl.pipeline.Stage):
    def run(self, input, output, benchmark):
        shutil.copy(input, output)
        space.shell(f"{local}/bin/alaska-transform-baseline", output)
        space.shell("llvm-dis", output)


perf_stats = [
    "instructions",
    "cycles",
    "duration_time",
    "cache-misses",
    "cache-references",
    # "stalled-cycles-backend",
    # "stalled-cycles-frontend",
    "L1-dcache-load-misses",
    "L1-dcache-loads",
    "L1-dcache-stores",
    "L1-icache-load-misses",
    "LLC-load-misses",
    "LLC-loads",
    "LLC-store-misses",
    "LLC-stores",
    "branch-instructions",
    "branch-misses",
    # "branch-load-misses",
    # "branch-loads",
    # "dTLB-load-misses",
    # "dTLB-loads",
    # "dTLB-store-misses",
    # "dTLB-stores",
    # "iTLB-load-misses",
    # "iTLB-loads",
    # "node-load-misses",
    # "node-loads",
    # "node-store-misses",
    # "node-stores",
]

class PerfRunner(Runner):
    def run(self, workspace, config, binary):
        cwd = os.getcwd() if config.cwd is None else config.cwd
        print("running cd", cwd, "&&", binary, *config.args)
        with waterline.utils.cd(cwd):
            proc = subprocess.Popen(
                ['perf', 'stat', '-e', ','.join(perf_stats), '-x', ',', '-o', f'{binary}.perf.csv', binary, *config.args],
                stdout=subprocess.DEVNULL,
                # stderr=subprocess.DEVNULL,
                env=config.env,
                cwd=cwd,
            )
            proc.wait()


        df = pd.read_csv(f'{binary}.perf.csv', comment='#', header=None)
        out = {}
        for val, name in zip(df[0], df[2]):
            out[name] = val
        # print(out)
        return out



# space.add_suite(wl.suites.Embench)
# space.add_suite(wl.suites.PolyBench, size="LARGE")
# # space.add_suite(wl.suites.Stockfish)
# space.add_suite(wl.suites.GAP, enable_openmp=enable_openmp, enable_exceptions=False)
# space.add_suite(wl.suites.NAS, enable_openmp=enable_openmp, suite_class="A")
space.add_suite(wl.suites.SPEC2017, tar="/home/nick/SPEC2017.tar.gz", config="ref")

space.clear_pipelines()

pl = waterline.pipeline.Pipeline("alaska")
pl.add_stage(waterline.pipeline.OptStage(['-O3']), name="Optimize")
pl.add_stage(AlaskaStage(), name="Alaska")
pl.set_linker(AlaskaLinker())
space.add_pipeline(pl)

pl = waterline.pipeline.Pipeline("baseline")
pl.add_stage(waterline.pipeline.OptStage(['-O3']), name="Optimize")
pl.add_stage(AlaskaBaselineStage(), name="Baseline")
pl.set_linker(AlaskaLinker())
space.add_pipeline(pl)



compile = True
results = space.run(runner=PerfRunner(), runs=1, compile=compile)
print(results)
results.to_csv("bench/results.csv", index=False)
# exit()

def plot_results(df, metric, baseline, modified, title='Result', ylabel='speedup'):
    df = df.pivot_table(index=['suite', 'benchmark'], columns='config', values=metric).reset_index()
    df[ylabel] = df[baseline] / df[modified]
    df['key'] = df['suite'] + '@' + df['benchmark']


    f, ax = plt.subplots(1, figsize=(13, 5))
    plt.grid(axis='y')

    g = sns.barplot(data=df,
                    errorbar="sd",
                    x='key',
                    y=ylabel,
                    hue='suite',
                    palette="Set2",
                    ax=ax,
                    dodge=False,
                    hatch='#')

    # Draw a line at 1
    ax.axhline(1, ls='--', zorder=13, color='black', linewidth=2)

    g.set_xticklabels(map(lambda x: x.split('@')[1], (item.get_text() for item in g.get_xticklabels())))

    for patch in g.patches:
        if not math.isnan(patch.get_height()):
            g.annotate(f'{patch.get_height():.2f}x', (patch.get_x() + patch.get_width() / 2., 0),
                           ha = 'center', va = 'bottom',
                           xytext = (0, 10), rotation=90,
                           textcoords = 'offset points', zorder=11)


    g.set(title=title)
    g.set(xlabel='Benchmark')
    g.set(ylabel=ylabel)

    # g.set_ylim((0, 2))
    plt.xticks(rotation=90)
    plt.tight_layout()
    plt.show()
    file = f'bench/{metric}.pdf'

    print('saving pdf to', file)
    plt.savefig(file)

df = pd.read_csv('bench/results.csv')
# print(df)
plot_results(df, 'duration_time', 'baseline', 'alaska', title='Benchmark Speedup (Higher is better)', ylabel='speedup')
# plot_results(df, 'maxrss', 'alaska', 'baseline', title='Benchmark RSS (Lower is better)', ylabel='speedup')

# plot_results(df, 'duration_time', 'baseline', 'alaska', title='Benchmark Speedup (Higher is better)', ylabel='speedup')
# plot_results(df, 'L1-dcache-loads', 'alaska', 'baseline', title='Loads from Level 1 data cache', ylabel='increase')
# plot_results(df, 'L1-dcache-load-misses', 'alaska', 'baseline', title='Misses in Level 1 data cache', ylabel='increase')
# plot_results(df, 'branch-instructions', 'alaska', 'baseline', title='Branch Instructions', ylabel='increase')
# plot_results(df, 'branch-misses', 'alaska', 'baseline', title='Branch Misses', ylabel='increase')
# plot_results(df, 'dTLB-loads', 'alaska', 'baseline', title='Loads from data TLB', ylabel='increase')
# plot_results(df, 'dTLB-load-misses', 'alaska', 'baseline', title='Loads from data TLB (misses)', ylabel='increase')
# plot_results(df, 'instructions', 'alaska', 'baseline', title='Instruction count increase (Lower is better)', ylabel='increase in instruction count')
