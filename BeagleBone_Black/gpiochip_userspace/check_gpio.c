/*
 * gpio-event-mon - monitor GPIO line events from userspace
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/gpio.h>

#define BUTTON_GPIO_LINE	27
#define LED_GPIO_LINE		15

static void usage(char **argv)
{
	printf("--> Program check status of Button GPIO "
	       "line and changed status of LED GPIO line.\n");
	printf("--> Usage: %s <num of tries>\n", argv[0]);
}

static void poll_button(int num_of_checking,
		struct gpiohandle_data *data_led, struct gpioevent_request *req_button,
		struct gpiohandle_request *req_led) {
	int ret;
	int i = 0;

	while (i < num_of_checking) {
		struct gpioevent_data event;

		ret = read(req_button->fd, &event, sizeof(event));
		if (ret == -1) {
			if (errno == -EAGAIN) {
				perror("nothing available: ");
				continue;
			} else {
				ret = -errno;
				perror("Failed to read event: ");
				break;
			}
		}

		if (ret != sizeof(event)) {
			ret = -EIO;
			perror("Reading event failed :\n");
			break;
		}
		printf("GPIO EVENT %llu: ", event.timestamp);
		switch (event.id) {
		case GPIOEVENT_EVENT_FALLING_EDGE:
			printf("Falling edge\n");

			data_led->values[0] = !data_led->values[0];

			ret = ioctl(req_led->fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, data_led);
			if (ret == -1) {
				ret = -errno;
				perror("Failed to issue GPIOHANDLE_SET_LINE_VALUES_IOCTL: ");
			}
			printf("Change LED status\n");
			break;
		default:
			printf("unknown event\n");
		}
		i++;
	}
}

static int init_gpio_val(int num_of_checking) {
	struct gpioevent_request *req_button;
	struct gpiohandle_request *req_led;
	struct gpiohandle_data *data_button, *data_led;
	int fd_gpiochip0, fd_gpiochip1;
	int ret;

	fd_gpiochip0 = open("/dev/gpiochip0", 0);
	if (fd_gpiochip0 == -1) {
		ret = -errno;
		perror("Failed to open /dev/gpiochip0: ");
		return ret;
	}

	fd_gpiochip1 = open("/dev/gpiochip1", 0);

	if (fd_gpiochip1 == -1) {
		close(fd_gpiochip0);
		ret = -errno;
		perror("Failed to open /dev/gpiochip1: ");
		return ret;
	}

	req_button = (struct gpioevent_request *)malloc(sizeof(struct gpioevent_request));
	if (req_button == NULL) {
		perror("Error: malloc failed\n");
		goto exit_close_error;
	}

	req_button->lineoffset = BUTTON_GPIO_LINE;
	req_button->handleflags = GPIOHANDLE_REQUEST_INPUT;
	req_button->eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
	strcpy(req_button->consumer_label, "gpio-event-mon");

	req_led = (struct gpiohandle_request *)malloc(sizeof(struct gpiohandle_request));
	if (req_led == NULL) {
		perror("Error: malloc failed: ");
		goto exit_close_error;
	}

	req_led->lineoffsets[0] = LED_GPIO_LINE;
	req_led->flags = GPIOHANDLE_REQUEST_OUTPUT;
	req_led->lines = 1;
	strcpy(req_led->consumer_label, "gpio-event-mon");
//	memcpy(req_led->default_values, data_led, sizeof(req_led->default_values));

	ret = ioctl(fd_gpiochip0, GPIO_GET_LINEEVENT_IOCTL, req_button);
	if (ret == -1) {
		ret = -errno;
		perror("Failed to issue GET EVENT: "
			"IOCTL ");
		goto exit_close_error;
	}

	ret = ioctl(fd_gpiochip1, GPIO_GET_LINEHANDLE_IOCTL, req_led);
	if (ret == -1) {
		ret = -errno;
		perror("Failed to issue GET EVENT: "
			"IOCTL ");
		goto exit_close_error;
	}

	data_button = (struct gpiohandle_data *)malloc(sizeof(struct gpiohandle_data));
	if (data_button == NULL) {
		perror("Error: malloc failed ");
		goto exit_close_error;
	}

	ret = ioctl(req_button->fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, data_button);
	if (ret == -1) {
		ret = -errno;
		perror("Failed to issue req_button GPIOHANDLE "
		       "GET LINE VALUES IOCTL: ");
		goto exit_close_error;
	}

	data_led = (struct gpiohandle_data *)malloc(sizeof(struct gpiohandle_data));
	if (data_led == NULL) {
		perror("Error: malloc failed: ");
		goto exit_close_error;
	}

	ret = ioctl(req_led->fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, data_led);
	if (ret == -1) {
		ret = -errno;
		perror("Failed to issue req_led GPIOHANDLE "
		       "GET LINE VALUES IOCTL: ");
		goto exit_close_error;
	}

	printf("Monitoring line %d on /dev/gpiochip0\n", BUTTON_GPIO_LINE);
	printf("Initial line value: %d\n", data_button->values[BUTTON_GPIO_LINE]);

	poll_button(num_of_checking, data_led, req_button, req_led);

exit_close_error:
	if (close(fd_gpiochip0) == -1)
		perror("Failed to close GPIO character device file\n");
	if (close(fd_gpiochip1) == -1)
		perror("Failed to close GPIO character device file\n");
	return ret;
}

int main(int argc, char **argv)
{
	int num_of_checking;

	if (argc != 2) {
		usage(argv);
		return 1;
	}
	num_of_checking = atoi(argv[1]);

	return init_gpio_val(num_of_checking);
}
