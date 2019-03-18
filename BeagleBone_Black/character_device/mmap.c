#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define MEM_BASE1 0x9f200000
#define REG_BASE1  0x9f201000

#define MEM_BASE2 0x9f210000
#define REG_BASE2 0x9f211000

#define MEM_SIZE	0x1000

#define MAX_DEVICES	2

extern int errno;

struct my_device {
	uint32_t mem_base;
	uint32_t mem_size;
};

static struct my_device my_devices[MAX_DEVICES] = {{
	.mem_base = MEM_BASE1,
	.mem_size = MEM_SIZE,
	},
	{
	.mem_base = MEM_BASE2,
	.mem_size = MEM_SIZE,
	},
};

struct my_devs {
	const char *cdev;
};

static struct my_devs cdevs[MAX_DEVICES] = {
	{
	.cdev = "/dev/dummy/dummy0"
	},
	{
	.cdev = "/dev/dummy/dummy1"
	},
};

int usage(char **argv)
{
	printf("Program sends file to the specific device\n");
	printf("Usage: %s <device>\n", argv[0]);
	return -1;
}
#define MAX_STRING 40
int main(int argc, char **argv)
{
	volatile unsigned char *mem_addr = NULL;
	unsigned int i, device, fd;

	if (argc != 2) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device >= MAX_DEVICES)
		return usage(argv);

	fd = open(cdevs[device].cdev, O_RDWR);
	if (fd < 0) {
		printf("file open error %s\n",cdevs[device].cdev);
		return -1;
	}
	mem_addr = (unsigned char *) mmap(0, my_devices[device].mem_size,
				PROT_WRITE, MAP_SHARED, fd, my_devices[device].mem_base);

	my_devices[device].mem_base = (unsigned long)mem_addr;
	if(mem_addr == NULL)
	{
		printf("Can't mmap\n");
		return -1;
	}
	while (1) {
		printf("\n");
		for (i=0; i <MAX_STRING; i++) {
			printf("%02hhX", *mem_addr++);
			if ((unsigned long) mem_addr > my_devices[device].mem_base + my_devices[device].mem_size) {
				printf("\n");
				return 0;
			}
		}
	}
	return 1;
}
