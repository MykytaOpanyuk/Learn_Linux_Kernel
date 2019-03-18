
# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.

KERNELDIR=/home/mykyta/Learning_linux_kernel_BBB/BeagleBone_Black/linux
CROSS_COMPILING := /opt/gcc-linaro-7.3.1-2018.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
PWD := $(shell pwd)
obj-m = platform_test.o platform_cdev.o platform_misc.o

modules:
	make ARCH=arm CROSS_COMPILE=$(CROSS_COMPILING) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	make ARCH=arm CROSS_COMPILE=$(CROSS_COMPILING) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.symvers modules.order

.PHONY: modules modules_install clean

