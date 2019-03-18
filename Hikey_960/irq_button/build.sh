#!/bin/sh

working_dir=/home/mykyta/Hikey_960

export PATH=$working_dir/aosp/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PATH
export CROSS_COMPILE="ccache aarch64-linux-androidkernel-"
export ARCH=arm64
export KDIR=$working_dir/linux

make
