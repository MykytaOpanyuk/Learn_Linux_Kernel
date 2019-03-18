PWD := $(shell pwd)
KERNEL_SOURCE := /home/mykyta/Learning_linux_kernel_BBB/BeagleBone_Black/linux
CROSS_COMPILING := /opt/gcc-linaro-7.3.1-2018.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

obj-m = hello_module_arithmetic.o
all:
	make ARCH=arm CROSS_COMPILE=$(CROSS_COMPILING) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules
clean:
	make -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
