

unset LD_LIBRARY_PATH

ROOT=$RISCV

export CC=$ROOT/bin/riscv64-unknown-linux-gnu-gcc
export CXX=$ROOT/bin/riscv64-unknown-linux-gnu-g++
export AS=$ROOT/bin/riscv64-unknown-linux-gnu-as
export LD=$ROOT/bin/riscv64-unknown-linux-gnu-ld

make clean

REV="$(whoami)-$(git rev-parse --short HEAD)"
if [[ $(git diff --stat) != '' ]]; then
  REV="${REV}-dirty"
fi


cmake ../ \
      -DALASKA_REVISION="${REV}" \
      -DALASKA_ENABLE_COMPILER=OFF \
      -DALASKA_ENABLE_TESTING=OFF \
      -DALASKA_ENABLE_LOGGING=OFF \
      -DFTR_NO_TRACE=1 \
      -DALASKA_CORE_ONLY=ON \
      -DALASKA_YUKON=ON \
      -DALASKA_YUKON_NO_HARDWARE=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DALASKA_SIZE_BITS=32 \
      -DCMAKE_SYSROOT=$ROOT/sysroot

make -j
