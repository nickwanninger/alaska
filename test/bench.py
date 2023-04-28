import waterline as wl
import waterline.suites
import waterline.utils
import waterline.pipeline
import shutil
import os
from pathlib import Path


local = Path(os.path.realpath(os.path.dirname(__file__))).parent / "local"
os.environ["PATH"] = f"{local}/bin:{os.environ['PATH']}"
os.environ["LD_LIBRARY_PATH"] = f"{local}/lib:{os.environ['LD_LIBRARY_PATH']}"

space = wl.Workspace("bench")


space.add_suite(wl.suites.NAS, enable_openmp=False, suite_class="B")
# space.add_suite(wl.suites.SPEC2017, tar="/home/nick/SPEC2017.tar.gz", config="test")
# space.add_suite(wl.suites.Stockfish)
# space.add_suite(wl.suites.PolyBench, size="LARGE")
space.add_suite(wl.suites.Embench)
space.add_suite(wl.suites.GAP)

linker_flags = os.popen("alaska-config --ldflags").read().strip().split("\n")

print("linker flags:", linker_flags)


class AlaskaLinker(wl.Linker):
    command = "clang++"
    args = [f"-L{local}/lib", "-lalaska"]

    def link(self, ws, objects, output, args=[]):
        # it's pretty safe to link using clang++.
        print("linking with alaksa linker")
        ws.shell("clang++", *args, *linker_flags, *objects, "-o", output)


class AlaskaStage(wl.pipeline.Stage):
    def run(self, input: Path, output: Path):
        shutil.copy(input, output)
        space.shell(f"{local}/bin/alaska-transform", output)
        space.shell("llvm-dis", output)


class AlaskaBaselineStage(wl.pipeline.Stage):
    def run(self, input: Path, output: Path):
        shutil.copy(input, output)
        space.shell(f"{local}/bin/alaska-transform-baseline", output)
        space.shell("llvm-dis", output)



space.clear_pipelines()


pl = waterline.pipeline.Pipeline("baseline")
pl.add_stage(AlaskaBaselineStage(), name="Baseline")
space.add_pipeline(pl)

pl = waterline.pipeline.Pipeline("alaska")
pl.add_stage(AlaskaStage(), name="Alaska")
pl.set_linker(AlaskaLinker())
space.add_pipeline(pl)


compile = True

# results = space.run(runs=2, compile=False)
results = space.run(runs=1, compile=compile)
results.to_csv("out.csv", index=False)
