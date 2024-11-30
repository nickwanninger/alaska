

unset LD_LIBRARY_PATH


export CC=/opt/riscv/bin/riscv64-unknown-linux-gnu-gcc
export CXX=/opt/riscv/bin/riscv64-unknown-linux-gnu-g++
export AS=/opt/riscv/bin/riscv64-unknown-linux-gnu-as
export LD=/opt/riscv/bin/riscv64-unknown-linux-gnu-ld

make clean

cmake ../ \
      -DALASKA_ENABLE_COMPILER=OFF \
      -DALASKA_ENABLE_TESTING=OFF \
      -DALASKA_CORE_ONLY=ON \
      -DALASKA_SIZE_BITS=32 \
      -DCMAKE_SYSROOT=/opt/riscv/sysroot

make -j
