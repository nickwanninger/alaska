#!/usr/bin/bash
#
# cross_compile.sh: build the pieces another script needs to apply Alaska's
# handle-translation transform to RISC-V bitcode with `opt`. This script
# compiles no programs and runs no tests. It only produces:
#
#   build-cross/local/lib/Alaska.so   host opt pass plugin
#   build-cross/translate.bc          RISC-V bitcode of runtime/rt/translate.cpp
#                                     (alaska_translate & friends, no handle faults)
#
# Run from the repo root: tools/cross_compile.sh
#
# Requirements:
#   - LLVM/Clang 21 at /usr/lib/llvm-21/bin (put on PATH below) plus clang-21 /
#     clang++-21 for the host build. The consumer's opt/llvm-link/llc must
#     also be LLVM 21, or the plugin won't load / bitcode won't read.
#   - RISC-V GNU toolchain + sysroot at /opt/riscv. Clang uses it for headers
#     (--sysroot/--gcc-toolchain). Nothing is linked here.
#
# History (why the script looks like this):
#   - It used to build the RISC-V runtime (libalaska.so), run the full
#     pipeline (incl. alaska-replace) on test/list.c, and link/test binaries.
#     The runtime build failed on <libunwind.h> (no RISC-V libunwind).
#   - Requirements then narrowed to "translation only, no runtime, no
#     handle faults", and then to "just produce the plugin + bitcode". A
#     separate script does the cross compiling via opt. Don't re-add program
#     builds, linking, or tests here.
#
# ---------------------------------------------------------------------------
# SCOPE
# ---------------------------------------------------------------------------
# The consumer wants *only* translation: alaska_translate checks inlined at
# pointer uses. It does NOT want:
#   - alaska-replace (malloc->halloc, free->hfree, libc wrappers)
#   - alaska-tracking (safepoints, HandleFaultPass, PinTrackingPass)
#   - handle faults: translate.cpp is compiled with -DALASKA_NO_HANDLE_FAULTS.
#     That macro is an #ifndef guard in alaska_translate_uncond()
#     (runtime/rt/translate.cpp) added for this script. It removes the
#     should_software_fault() -> alaska::do_handle_fault_and_translate()
#     (asm name "alaska.HF") slow path, so translate.bc has no references
#     back into the runtime. Normal builds don't define it and are unchanged.
#     If that guard is ever removed, alaska.HF reappears as an undefined
#     symbol in transformed programs.
#   - the RISC-V runtime library (libalaska). Building it also needs a
#     cross-compiled libunwind (runtime/rt/barrier.cpp includes <libunwind.h>),
#     which we don't have. It is intentionally not built here.
#
# Resulting semantics: allocations stay plain libc, so no handles ever exist.
# Every inlined translate takes the "not a handle" branch
# ((int64_t)p >= 0 || p == -1 -> p). The overhead measured is check + branch
# + code-layout effects. The handle-table walk is compiled in but never runs.
#
# ---------------------------------------------------------------------------
# HOW THE CONSUMER SHOULD USE THESE FILES
# ---------------------------------------------------------------------------
# Given RISC-V bitcode prog.bc, compiled e.g. as
#   clang -target riscv64-unknown-linux-gnu --sysroot=/opt/riscv/sysroot \
#     --gcc-toolchain=/opt/riscv -g0 -O3 -DALASKA_SIZE_BITS=32 \
#     -c -emit-llvm prog.c -o prog.bc
# (ALASKA_SIZE_BITS must match TRANSLATE_CFLAGS below. It sets how many low
# bits of a handle are the offset. Multi-file programs: llvm-link them into
# one prog.bc first, because escape/hoisting are module-level):
#
#   P=build-cross/local/lib/Alaska.so
#   opt prog.bc -o prog.bc \
#     -passes=mergereturn,break-crit-edges,loop-simplify,lcssa,indvars,mem2reg,instnamer
#   for p in alaska-prepare alaska-translate alaska-escape alaska-lower; do
#     opt --load-pass-plugin=$P --passes=$p prog.bc -o prog.bc
#   done
#   llvm-link prog.bc --only-needed --internalize build-cross/translate.bc -o prog.bc
#   opt --load-pass-plugin=$P --passes=alaska-inline,globaldce prog.bc -o prog.bc
#   # then, e.g.:
#   llc -O3 -mtriple=riscv64-unknown-linux-gnu -filetype=obj prog.bc -o prog.o
#   riscv64-unknown-linux-gnu-gcc prog.o -o prog -lm -lpthread
#
# For an untransformed baseline, snapshot prog.bc after the canonicalization
# opt line and before the Alaska passes, then give it the same llc/link
# steps. That way the two differ only by Alaska. There is no IR-level
# opt -O3 after the Alaska passes. If you add one, add it to both.
#
# Notes on that pipeline:
#   - One opt invocation per Alaska pass. That is how the project drives them.
#   - alaska-prepare    DCE/ADCE, devirt, marks "alaska_is_simple", normalize
#     alaska-translate  insert + hoist alaska.translate/release intrinsics
#                       (alaska-translate-nohoist = no hoisting). It also runs
#                       AlaskaIntentPass, a debug no-op unless a fn is named
#                       "search".
#     alaska-escape     translate pointers passed to external fns (printf,
#                       memcpy, ...). Omit it for loads/stores only.
#     alaska-lower      alaska.translate -> call @alaska_translate
#     llvm-link         MUST come after alaska-lower. Before lowering nothing
#                       references @alaska_translate, so --only-needed would
#                       pull in nothing.
#     alaska-inline     inline all calls to alaska_* functions
#     globaldce         drop the dead internal translate definitions
#   - Self-check: after llc, `llvm-nm -u` on the object should list only libc
#     symbols. alaska.HF / halloc / hfree / alaska_* mean the runtime leaked
#     back in.
#   - The result links with plain libc (no libalaska, no Alaska linker
#     script). Programs in test/ that call alaska_timestamp() need the
#     consumer to supply it (a clock_gettime wrapper).
#   - translate.bc still declares alaska_barrier_poll (used by
#     alaska_safepoint). --only-needed leaves it out unless safepoints are
#     inserted, and they aren't, because alaska-tracking is not run.
#   - "preserve_all/preserve_most not supported for this target" warnings
#     are expected on RISC-V and harmless (they only affect the fault path).
#   - Neither alaska-replace nor alaska-tracking should appear in the
#     pipeline (see SCOPE).

set -e

export PATH=/usr/lib/llvm-21/bin:$PATH

ROOT=$(pwd)
B=${ROOT}/build-cross
L=${B}/local

mkdir -p ${B}

# --- Host build of the compiler plugin (Alaska.so) --------------------------
# The plugin runs inside host opt, so it is built for the host. It transforms
# IR independently of the target, so it works on RISC-V bitcode unchanged.
# The host runtime this build also produces is unused. cmake only configures
# once (guarded by Makefile), so delete build-cross/host to pick up option
# changes.
mkdir -p ${B}/host
pushd ${B}/host
  export CC=clang-21
  export CXX=clang++-21
  # Use lld: it handles LTO natively, so the LTO/IPO configure check does not
  # have to dlopen LLVMgold.so through whichever ld happens to be on PATH.
  # Must be LDFLAGS (not -DCMAKE_*_LINKER_FLAGS) so check_ipo_supported()'s
  # try-compile project picks it up too.
  export LDFLAGS="-fuse-ld=lld"

  echo "Building Alaska plugin for host..."
  if [ ! -f Makefile ]; then
      cmake ../../ -DALASKA_ENABLE_TESTING=OFF -DCMAKE_INSTALL_PREFIX:PATH=${L}
  fi
  make -j 32 install
popd
unset LDFLAGS

# --- RISC-V translate.bc -----------------------------------------------------
# ALASKA_SIZE_BITS must match what the consumer compiles programs with.
# -include config.h / -I./runtime: translate.cpp needs the alaska headers.
# -g0 + --strip-debug: debug info only gets in the way of linking/inlining.
TRANSLATE_CFLAGS="-target riscv64-unknown-linux-gnu --sysroot=/opt/riscv/sysroot --gcc-toolchain=/opt/riscv "
TRANSLATE_CFLAGS+="-g0 -fno-sanitize=cfi -O3 -DALASKA_SIZE_BITS=32 -include runtime/alaska/config.h -I./runtime "
TRANSLATE_CFLAGS+="-DALASKA_NO_HANDLE_FAULTS "

echo "Building translate.bc for RISC-V..."
clang++ ${TRANSLATE_CFLAGS} runtime/rt/translate.cpp -c -emit-llvm -o ${B}/translate.bc
opt --strip-debug ${B}/translate.bc -o ${B}/translate.bc

echo
echo "plugin:    ${L}/lib/Alaska.so"
echo "translate: ${B}/translate.bc"
