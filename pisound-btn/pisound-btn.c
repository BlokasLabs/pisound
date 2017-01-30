/*
 * pisound-btn daemon for the pisound button.
 * Copyright (C) 2016  Vilniaus Blokas UAB, http://blokas.io/pisound
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <time.h>

#define HOMEPAGE_URL "http://blokas.io/pisound"

enum { PISOUND_BTN_VERSION     = 0x0101 };
enum { INVALID_VERSION         = 0xffff };
enum { BUTTON_PIN              = 17     };
enum { CLICK_TIMEOUT_MS        = 300    };
enum { HOLD_PRESS_TIMEOUT_MS   = CLICK_TIMEOUT_MS };
enum { PRESS_COUNT_LIMIT       = 3      };

#define SCRIPTS_DIR "/usr/local/etc/pisound"

static const char *const DOWN_ACTION          = SCRIPTS_DIR "/down.sh";         // Executed every time the button is pushed down.
static const char *const UP_ACTION            = SCRIPTS_DIR "/up.sh";           // Executed every time the button is released up.
static const char *const SINGLE_CLICK_ACTION  = SCRIPTS_DIR "/single_click.sh"; // Executed if the button was clicked and released once in timeout.
static const char *const DOUBLE_CLICK_ACTION  = SCRIPTS_DIR "/double_click.sh"; // Executed if the button was double clicked within given timeout.
static const char *const TRIPPLE_CLICK_ACTION = SCRIPTS_DIR "/tripple_click.sh";// Executed if the button was tripple clicke within given timeout.
static const char *const HOLD_ACTION          = SCRIPTS_DIR "/hold.sh";         // Executed if the button was held for given time.

static const char *const PISOUND_VERSION_FILE = "/sys/kernel/pisound/version";

int gpio_is_pin_valid(int pin)
{
	return pin >= 0 && pin < 100;
}

enum edge_e
{
	E_NONE    = 0,
	E_RISING  = 1,
	E_FALLING = 2,
	E_BOTH    = 3,
};

int gpio_set_edge(int pin, enum edge_e edge)
{
	if (!gpio_is_pin_valid(pin))
	{
		fprintf(stderr, "Invalid pin number %d!\n", pin);
		return -1;
	}

	char gpio[64];

	snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%d/edge", pin);

	int fd = open(gpio, O_WRONLY);
	if (fd == -1)
	{
		fprintf(stderr, "Failed to open %s! Error %d.\n", gpio, errno);
		return -1;
	}

	static const char *const edge2str[] =
	{
		"none",
		"rising",
		"falling",
		"both",
	};

	const int n = strlen(edge2str[edge])+1;

	int result = write(fd, edge2str[edge], n);
	if (result != n)
	{
		fprintf(stderr, "Failed writing to %s! Error %d.\n", gpio, errno);
		close(fd);
		return -1;
	}
	int err = close(fd);
	if (err != 0)
	{
		fprintf(stderr, "Failed closing %s! Error %d.\n", gpio, errno);
		return -1;
	}
	return 0;
}

int gpio_open(int pin)
{
	if (!gpio_is_pin_valid(pin))
	{
		fprintf(stderr, "Invalid pin number %d!\n", pin);
		return -1;
	}

	char gpio[64];

	snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%d/value", pin);

	int fd = open(gpio, O_RDONLY);

	if (fd == -1)
	{
		fprintf(stderr, "Failed opening %s! Error %d.\n", gpio, errno);
	}

	return fd;
}

int gpio_close(int fd)
{
	int err = close(fd);
	if (err != 0)
	{
		fprintf(stderr, "Failed closing descriptor %d! Error %d.\n", fd, err);
		return -1;
	}
	return 0;
}

typedef unsigned long long timestamp_ms_t;

timestamp_ms_t get_timestamp_ms(void)
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

unsigned short get_kernel_module_version(void)
{
	FILE *f = fopen(PISOUND_VERSION_FILE, "rt");

	if (!f)
		return INVALID_VERSION;

	unsigned int major;
	unsigned int minor;
	int n = fscanf(f, "%u.%u", &major, &minor);
	fclose(f);

	if (n == 2)
		return ((major & 0xff) << 8) + (minor & 0xff);

	return INVALID_VERSION;
}

static void onTimesClicked(unsigned num_presses)
{
	switch (num_presses)
	{
	case 1:
		system(SINGLE_CLICK_ACTION);
		break;
	case 2:
		system(DOUBLE_CLICK_ACTION);
		break;
	case 3:
		system(TRIPPLE_CLICK_ACTION);
		break;
	default:
		fprintf(stderr, "onTimesClicked(%u) called, unexpected!\n", num_presses);
		return;
	}
}

static void onDown(void)
{
	system(DOWN_ACTION);
}

static void onUp(void)
{
	system(UP_ACTION);
}

static void onHold(unsigned num_presses)
{
	char cmd[64];
	sprintf(cmd, "%s %u", HOLD_ACTION, num_presses);
	system(cmd);
}

int run(void)
{
	unsigned short version = get_kernel_module_version();

	if (version == INVALID_VERSION)
	{
		fprintf(stderr, "Reading pisound version failed, did the kernel module load successfully?\n");
		return -EINVAL;
	}
	else if (version < 0x0100 || version >= 0x0200)
	{
		fprintf(stderr, "The kernel module version (%04x) and pisound-btn version (%04x) are incompatible! Please check for updates at " HOMEPAGE_URL "\n", version, PISOUND_BTN_VERSION);
		return -EINVAL;
	}

	int err = gpio_set_edge(BUTTON_PIN, E_BOTH);

	if (err != 0)
		return err;

	enum
	{
		FD_BUTTON = 0,
		FD_TIMER  = 1,
		FD_COUNT
	};

	int btnfd = gpio_open(BUTTON_PIN);
	if (btnfd == -1)
		return errno;

	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timerfd == -1)
	{
		fprintf(stderr, "Creating timer failed. Error %d.\n", errno);
		gpio_close(btnfd);
		return errno;
	}

	struct pollfd pfd[FD_COUNT];

	pfd[FD_BUTTON].fd = btnfd;
	pfd[FD_BUTTON].events = POLLPRI;

	pfd[FD_TIMER].fd = timerfd;
	pfd[FD_TIMER].events = POLLIN;

	timestamp_ms_t pressed_at = 0;

	bool timer_running = false;
	bool button_down = false;
	unsigned num_pressed = 0;

	for (;;)
	{
		int result = poll(pfd, FD_COUNT, -1);

		if (result == -1)
			break;

		if (result == 0)
			continue;

		if (pfd[FD_BUTTON].revents & POLLPRI) // Button state changed.
		{
			timestamp_ms_t timestamp = get_timestamp_ms();

			char buff[16];
			memset(buff, 0, sizeof(buff));
			int n = read(btnfd, buff, sizeof(buff));
			if (n == 0)
			{
				fprintf(stderr, "Reading button value returned 0.\n");
				break;
			}

			if (lseek(btnfd, SEEK_SET, 0) == -1)
			{
				fprintf(stderr, "Rewinding button failed. Error %d.\n", errno);
				break;
			}

			unsigned long pressed = strtoul(buff, NULL, 10);

			if (pressed)
			{
				button_down = true;
				onDown();

				if (!timer_running)
				{
					num_pressed = 1;
					timer_running = true;
				}
				else
				{
					if (num_pressed < PRESS_COUNT_LIMIT)
						++num_pressed;
				}

				pressed_at = timestamp;

				struct itimerspec its;
				memset(&its, 0, sizeof(its));

				its.it_value.tv_sec = 0;
				its.it_value.tv_nsec = CLICK_TIMEOUT_MS * 1000 * 1000;
				timerfd_settime(timerfd, 0, &its, 0); // Start the timer.
			}
			else if (button_down)
			{
				button_down = false;
				onUp();

				if (pressed_at != 0)
				{
					if (timestamp - pressed_at >= HOLD_PRESS_TIMEOUT_MS)
					{
						onHold(num_pressed);
					}
				}
			}

		}
		if (pfd[FD_TIMER].revents & POLLIN) // Timer timed out.
		{
			uint64_t t;
			int n = read(timerfd, &t, sizeof(t));
			if (n != sizeof(t))
			{
				fprintf(stderr, "Error %d reading the timer!\n", errno);
				return errno;
			}
			if (!button_down)
				onTimesClicked(num_pressed);
			timer_running = false;
		}
	}

	close(timerfd);
	gpio_close(btnfd);

	gpio_set_edge(BUTTON_PIN, E_NONE);
	return 0;
}

void print_version(void)
{
	printf("Version %x.%02x, Blokas Labs " HOMEPAGE_URL "\n", PISOUND_BTN_VERSION >> 8, PISOUND_BTN_VERSION & 0xff);
}

void print_usage(void)
{
	printf("Usage: pisound-btn [options]\n"
		"Options:\n"
		"\t--help     Display the usage information.\n"
		"\t--version  Show the version information.\n"
		"\n"
		);
	print_version();
}

int main(int argc, char **argv)
{
	int i;
	for (i=1; i<argc; ++i)
	{
		if (strcmp(argv[i], "--help") == 0)
		{
			print_usage();
			return 0;
		}
		else if (strcmp(argv[i], "--version") == 0)
		{
			print_version();
			return 0;
		}
		else
		{
			printf("Unknown option '%s'.\n", argv[i]);
			print_usage();
			return 0;
		}
	}

	return run();
}
