#!/bin/sh

working_dir=/home/mykyta/Hikey_960

# Use Linux toolchain as we're gonna build user-space app as well
export PATH=/opt/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin:$PATH
export CROSS_COMPILE="ccache aarch64-linux-gnu-"
export ARCH=arm64
export KDIR=$working_dir/linux

make $*
