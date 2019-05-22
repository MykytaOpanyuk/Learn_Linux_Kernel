PWD := $(shell pwd)
CC := /opt/gcc-linaro-7.3.1-2018.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
check_gpio: check_gpio.o
	$(CC) -o check_gpio check_gpio.o	

.PHONY: clean

clean:
	rm -f *.o
