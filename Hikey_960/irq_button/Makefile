ifneq ($(KERNELRELEASE),)

# kbuild part of makefile

CFLAGS_hello.o := -DDEBUG
obj-m := irq_button.o

else

# normal makefile

KDIR ?= /lib/modules/$(shell uname -r)/build

module:
	$(MAKE) -C $(KDIR) M=$(PWD) C=1 modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) C=1 clean

.PHONY: module clean

endif
