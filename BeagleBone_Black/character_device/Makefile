ifneq ($(KERNELRELEASE),)

# kbuild part of makefile

CFLAGS_hw2.o := -DDEBUG
obj-m := platform_test.o platform_misc.o platform_cdev.o

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
