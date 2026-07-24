#!/bin/bash

export BINUTILS_VERSION=2.45.1
export GCC_VERSION=15.2.0

wget https://ftpmirror.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz

mkdir binutils-build
cd binutils-build
../binutils-${BINUTILS_VERSION}/configure --target=x86_64-monolith --prefix=${PREFIX} --with-sysroot=${SYSROOT} --disable-werror --enable-default-execstack=no
make -j${NPROC}
make install
cd..

wget https://ftpmirror.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz

mkdir gcc-build
cd gcc-build
../gcc-${GCC_VERSION}/configure --target=x86_64-monolith --prefix=${PREFIX} --with-sysroot=${SYSROOT} --enable-languages=c,c++, --enable-threads=posix --disable-multilib --enable-shared --enable-host-shared --with-pic --disable-nls --disable-gcov
make all-gcc -j${NPROC}
make all-target-libgcc -j${NPROC}
make install-gcc
make install-target-libgcc
cd ..
