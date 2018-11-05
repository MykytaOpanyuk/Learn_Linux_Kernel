PWD := $(shell pwd)
KERNEL_SOURCE := /home/mopaniuk/test_kernel/linux
obj-m = ioctl_example.o
all:
	make -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules
clean:
	make -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
