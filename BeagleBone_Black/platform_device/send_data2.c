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

#define MEM_SIZE	0x1000
#define REG_SIZE	8

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_I_DATA_READY		(1) /*IO data ready flag */

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
	printf("Program sends file to the specific device\n");
	printf("Usage: %s <device> <file>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	volatile unsigned int *read_reg_addr = NULL, *read_count_addr, *read_flag_addr;
	volatile unsigned char *read_mem_addr = NULL;
	unsigned int i, device, ret, len, count;
	struct stat st;
	uint8_t *buf, *timetable;
	FILE *f;

	if (argc != 3) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device >= MAX_DEVICES)
		return usage(argv);

	ret = stat(argv[2], &st);
	if (ret) {
		printf("ERROR: Can't find (%s)\n", argv[2]);
		return -1;
	}
	buf = (uint8_t *) malloc(st.st_size);
	if (!buf) {
		printf("ERROR: Out of memory");
		return -1;
	}

	f = fopen(argv[2], "rb");
	if (!f) {
		printf("fopen error (%s)\n", argv[2]);
		return -1;
	}
	len = fread(buf, 1U, st.st_size, f);
	fclose(f);

	timetable = buf;
	printf("Buffer: \n");
	for (i = 0; i < len; i++) {
		printf("%u, ", *timetable);
		timetable++;
		
		if (i % 16 == 0) 
			printf("\n");
	}

	timetable = buf;

	if (len != st.st_size) {
		printf("File read Error (%s)\n", argv[2]);
		return -1;
	}

	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	if(fd < 0) {
		printf("Can't open /dev/mem\n");
		return -1;
	}

	read_mem_addr = (unsigned char *) mmap(0, my_devices[device].read_mem_size,
				PROT_WRITE, MAP_SHARED, fd, my_devices[device].read_mem_base);
	
	if(read_mem_addr == NULL) {
		printf("Can't mmap\n");
		return -1;
	}

	read_reg_addr = (unsigned int *) mmap(0, my_devices[device].read_reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].read_reg_base);

	read_flag_addr = read_reg_addr;
	read_count_addr = read_reg_addr;
	read_count_addr++;

	*read_flag_addr = 0;
	while (len) {
		while (*read_flag_addr & PLAT_I_DATA_READY)
			usleep(50000);
		
		count = len > my_devices[device].read_mem_size ? 
				my_devices[device].read_mem_size : len;
		memcpy((void *)read_mem_addr, buf, count);
		len -= count;
		buf += count;
		*read_count_addr = count;
		*read_flag_addr = PLAT_I_DATA_READY;
	}

	close(fd);
	free(timetable);

	return 0;
}
