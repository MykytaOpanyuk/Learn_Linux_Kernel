#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "hw3.h"
#include "fcntl.h"
#include <poll.h>

#define TIMEOUT 2

int main(void)
{
	struct pollfd fd_str;
	int i;
	int *led_state = (int *)malloc(sizeof(int));
	int result;
	*led_state = 0;

	if ((fd_str.fd = open("/dev/hw3", O_RDWR))<0) {
		printf("Open error on /dev/hw3\n");
		return 1;
	}
	fd_str.events =  POLLIN | POLLRDNORM;

	result = ioctl(fd_str.fd, HW3IOC_KERN_CONTROL, led_state);

	printf("Control : %d\n", *led_state);

	if (result != 0)
		printf("ERROR - ioctl HW3IOC_KERN_CONTROL\n");

	printf("Set LED status\n");
	*led_state = 1;
	result = ioctl(fd_str.fd, HW3IOC_SETLED, led_state);

	if (result != 0)
		printf("ERROR - ioctl HW3IOC_SETLED\n");

	result = read(fd_str.fd, led_state, 4);
	if (result != 0)
		printf("ERROR - ioctl read %d\n", result);

	printf("LED status : %d", *led_state);

	while (1) {
		result = poll(&fd_str, POLLIN | POLLRDNORM, TIMEOUT);

		if (result ==  (POLLIN | POLLRDNORM)) {
			for (i = 0; i < 10; i++) {
				printf("Led ON\n");
				*led_state = 1;
				ioctl(fd_str.fd,HW3IOC_SETLED,led_state);
				sleep(2);

				printf("Led OFF\n");
				*led_state = 0;
				ioctl(fd_str.fd,HW3IOC_SETLED,led_state);
				sleep(2);
			}
		}
	}
	close(fd_str.fd);
	free(led_state);
	return 0;
}
