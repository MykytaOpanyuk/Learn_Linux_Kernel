PWD := $(shell pwd)
KERNEL_SOURCE := /home/mopaniuk/test_kernel/linux
obj-m = my_proc.o
all:
	make -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules
clean:
	make -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
