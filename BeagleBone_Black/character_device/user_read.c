#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "fcntl.h"
#include <poll.h>

#define MAX_DEVICES		2
#define MEM_SIZE		0x1000

int usage(char **argv)
{
	printf("--> Program read from the specific device\n");
	printf("--> Usage: %s <device>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	struct pollfd fd_str;
	unsigned int i, device;
	uint8_t *buf, *timetable;
	int result;

	if (argc != 2) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device > MAX_DEVICES)
		return usage(argv);
	printf("--> device: %u\n", device);

	if (device == 0) {
		if ((fd_str.fd = open("/dev/dummy/dummy0", O_RDWR)) < 0) {
			printf("Open error on /dev/dummy/dummy0\n");
			return 1;
		}
		printf("--> Get file descriptor of dummy0\n");
	}
	else {
		if ((fd_str.fd = open("/dev/dummy/dummy1", O_RDWR)) < 0) {
			printf("Open error on /dev/dummy/dummy1\n");
			return 1;
		}
		printf("--> Get file descriptor of dummy1\n");
	}

	buf = (uint8_t *) malloc(MEM_SIZE);
	printf("--> Try to alloc buffer\n");

	if (!buf) {
		printf("--> ERROR: Out of memory");
		return -1;
	}

	result = read(fd_str.fd, buf, MEM_SIZE);

	if (result != 0)
		printf("--> ERROR - read dummy dev %d\n", result);

//	timetable = buf;
//	printf("--> Buffer: \n");
//	for (i = 0; i < MEM_SIZE; i++) {
//		printf("%u ", *timetable);
//		timetable++;

//		if ((i % 16 == 0) && (i != 0))
//			printf("\n");
//	}
//	printf("\n");

	free(buf);
	close(fd_str.fd);
	return 0;
}
