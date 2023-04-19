import waterline as wl
import waterline.suites
import waterline.utils
import waterline.pipeline


space = wl.Workspace("bench")

space.add_suite(wl.suites.NAS, enable_openmp=True, suite_class="A")
space.add_suite(wl.suites.GAP)
space.add_suite(wl.suites.PolyBench, size="SMALL")

space.run(runs=0)

