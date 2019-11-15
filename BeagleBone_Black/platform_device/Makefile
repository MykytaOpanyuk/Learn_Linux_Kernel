ifneq ($(KERNELRELEASE),)

# kbuild part of makefile

CFLAGS_hw2.o := -DDEBUG
obj-m := platform_test_dts.o
# Use multi_v7_defconfig as a base for merge_config.sh

else

# normal makefile

KDIR ?= /lib/modules/$(shell uname -r)/build

module:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILING) -C $(KDIR) M=$(PWD) C=1 modules
clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILING) -C $(KDIR) M=$(PWD) C=1 clean
# USER_C - C userspace program	
user:
	$(CROSS_COMPILING) $(USER_C) -o $(USER)
.PHONY: module clean

endif

