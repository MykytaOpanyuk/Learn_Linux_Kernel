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



#define READ_MEM_BASE1		0x9f200000
#define READ_REG_BASE1		0x9f201000
#define WRITE_MEM_BASE1		0x9f202000
#define WRITE_REG_BASE1		0x9f203000

#define READ_MEM_BASE2		0x9f210000
#define READ_REG_BASE2		0x9f211000
#define WRITE_MEM_BASE2		0x9f212000
#define WRITE_REG_BASE2		0x9f213000

#define MEM_SIZE		0x1000
#define REG_SIZE		8

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_O_DATA_READY		(1) /*IO data ready flag */

#define MAX_DEVICES	2

extern int errno;

struct my_device {
	uint32_t read_mem_base;
	uint32_t read_mem_size;
	uint32_t read_reg_base;
	uint32_t read_reg_size;

	uint32_t write_mem_base;
	uint32_t write_mem_size;
	uint32_t write_reg_base;
	uint32_t write_reg_size;
};

static struct my_device my_devices[MAX_DEVICES] = {{
	.read_mem_base = READ_MEM_BASE1,
	.read_mem_size = MEM_SIZE,
	.read_reg_base = READ_REG_BASE1,
	.read_reg_size = REG_SIZE,
	.write_mem_base = WRITE_MEM_BASE1,
	.write_mem_size = MEM_SIZE,
	.write_reg_base = WRITE_REG_BASE1,
	.write_reg_size = REG_SIZE,
	},
	{
	.read_mem_base = READ_MEM_BASE2,
	.read_mem_size = MEM_SIZE,
	.read_reg_base = READ_REG_BASE2,
	.read_reg_size = REG_SIZE,
	.write_mem_base = WRITE_MEM_BASE2,
	.write_mem_size = MEM_SIZE,
	.write_reg_base = WRITE_REG_BASE2,
	.write_reg_size = REG_SIZE,
	},
};
int usage(char **argv)
{
	printf("--> Program write into file from the specific device\n");
	printf("--> Usage: %s <device> <file>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	volatile unsigned int *write_reg_addr = NULL, *write_count_addr, *write_flag_addr;
	volatile unsigned char *write_mem_addr = NULL;
	unsigned int i, device, count;
	uint8_t *buf, *timetable;
	uint32_t size_of_buf = 0;
	FILE *f;

	if (argc != 3)
		return usage(argv);

	device = atoi(argv[1]);
	if (device > MAX_DEVICES)
		return usage(argv);
	printf("--> device: %u\n", device);

	f = fopen(argv[2], "ab");
	if (!f) {
		printf("--> fopen error (%s)\n", argv[2]);
		return -1;
	}

	int fd = open("/dev/mem", O_RDWR|O_SYNC);

	if(fd < 0) {
		printf("--> Can't open /dev/mem\n");
		return -1;
	}

	write_mem_addr = (unsigned char *) mmap(0, my_devices[device].write_mem_size,
				PROT_READ, MAP_SHARED, fd, my_devices[device].write_mem_base);

	if(write_mem_addr == NULL) {
		printf("--> Can't mmap\n");
		return -1;
	}

	write_reg_addr = (unsigned int *) mmap(0, my_devices[device].write_reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].write_reg_base);

	write_flag_addr = write_reg_addr;
	write_count_addr = write_reg_addr;
	write_count_addr++;
	size_of_buf = *write_count_addr;
	buf = (uint8_t *) malloc(MEM_SIZE);
	if (!buf) {
		printf("--> ERROR: Out of memory");
		return -1;
	}

	printf("--> write_flag_addreadr value: %d\n", *write_flag_addr);
	printf("--> write_count_addr value: %d\n", *write_count_addr);

	while (size_of_buf > 0) {
		*write_flag_addr = PLAT_O_DATA_READY;
		while (*write_flag_addr & PLAT_O_DATA_READY)
			usleep(5000);

		printf("my_devices[device].write_mem_size : %u\n", *write_count_addr);
		count = *write_count_addr > MEM_SIZE ?
				MEM_SIZE : *write_count_addr;
		memcpy((void *)buf, write_mem_addr, count);
		printf("--> count : %u\n", count);
		fwrite(buf, sizeof(char), MEM_SIZE, f);
		size_of_buf -= count;
	}

	timetable = buf;
	printf("--> Buffer: \n");
	for (i = 0; i < count; i++) {
		printf("%u ", *timetable);
		timetable++;

		if ((i % 16 == 0) && (i != 0))
			printf("\n");
	}
	printf("\n");
	fclose(f);
	close(fd);
	free(buf);

	return 0;
}

