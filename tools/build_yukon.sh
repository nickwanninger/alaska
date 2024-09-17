

unset LD_LIBRARY_PATH

ROOT=$RISCV

export CC=$ROOT/bin/riscv64-unknown-linux-gnu-gcc
export CXX=$ROOT/bin/riscv64-unknown-linux-gnu-g++
export AS=$ROOT/bin/riscv64-unknown-linux-gnu-as
export LD=$ROOT/bin/riscv64-unknown-linux-gnu-ld

make clean

cmake ../ \
      -DALASKA_ENABLE_COMPILER=OFF \
      -DALASKA_ENABLE_TESTING=OFF \
      -DALASKA_CORE_ONLY=ON \
      -DALASKA_YUKON=ON \
      -DALASKA_SIZE_BITS=24 \
      -DCMAKE_SYSROOT=$ROOT/sysroot

make -j
