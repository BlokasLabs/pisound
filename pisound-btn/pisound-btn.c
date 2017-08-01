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

#define HOMEPAGE_URL "https://blokas.io/pisound"
#define UPDATE_URL   HOMEPAGE_URL "/updates?btnv=%x.%02x&v=%s&sn=%s&id=%s"

enum { PISOUND_BTN_VERSION     = 0x0103 };
enum { INVALID_VERSION         = 0xffff };
enum { BUTTON_PIN              = 17     };
enum { CLICK_TIMEOUT_MS        = 300    };
enum { HOLD_PRESS_TIMEOUT_MS   = CLICK_TIMEOUT_MS };
enum { PRESS_COUNT_LIMIT       = 8      };

#define SCRIPTS_DIR "/usr/local/etc/pisound"

static const char *const DOWN_ACTION          = SCRIPTS_DIR "/down.sh";         // Executed every time the button is pushed down.
static const char *const UP_ACTION            = SCRIPTS_DIR "/up.sh";           // Executed every time the button is released up.
static const char *const CLICK_ACTION         = SCRIPTS_DIR "/click.sh";        // Executed when the button is short-clicked one or multiple times in quick succession.
static const char *const HOLD_ACTION          = SCRIPTS_DIR "/hold.sh";         // Executed if the button was held for given time.

static const char *const PISOUND_ID_FILE      = "/sys/kernel/pisound/id";
static const char *const PISOUND_SERIAL_FILE  = "/sys/kernel/pisound/serial";
static const char *const PISOUND_VERSION_FILE = "/sys/kernel/pisound/version";

static const char *const UPDATE_CHECK_DISABLE_FILE = SCRIPTS_DIR "/disable_update_check"; // If the file exists, the update check will be disabled.

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

static bool is_update_check_enabled()
{
	struct stat s;
	return stat(UPDATE_CHECK_DISABLE_FILE, &s) != 0;
}

static bool parse_version(unsigned short *result, const char *version)
{
	unsigned int major;
	unsigned int minor;
	int n = sscanf(version, "%u.%u", &major, &minor);

	if (n == 2)
	{
		*result = ((major & 0xff) << 8) + (minor & 0xff);
		return true;
	}

	*result = INVALID_VERSION;
	return false;
}

static void check_for_updates(unsigned short btn_version, const char *version, const char *serial, const char *id)
{
	char url[512];
	if (snprintf(url, sizeof(url), UPDATE_URL, (btn_version & 0xff00) >> 8, btn_version & 0xff, version, serial, id) < 0)
		return;

	char cmd[1024];
	if (snprintf(cmd, sizeof(cmd), "sleep 30 && wget \"%s\" -O - > /dev/null 2>&1 &", url) < 0)
		return;

	system(cmd);
}

static bool read_text_file(char *dst, size_t buffer_size, size_t *bytes_read, const char *file)
{
	if (!dst || !file || buffer_size == 0)
		return false;

	*dst = '\0';
	*bytes_read = 0;

	FILE *f = fopen(file, "rt");
	if (!f)
		return false;

	char buff[1024];
	size_t current_index = 0;
	while (!feof(f) && current_index < (buffer_size-1))
	{
		size_t n = fread(buff, sizeof(buff[0]), sizeof(buff)/sizeof(buff[0]), f);

		size_t space_available = buffer_size - 1 - current_index;
		size_t to_copy = n > space_available ? space_available : n;
		memcpy(&dst[current_index], buff, to_copy);

		current_index += to_copy;
	}

	fclose(f);

	*bytes_read = current_index;
	dst[current_index] = '\0';

	return true;
}

static bool read_pisound_system_file(char *dst, size_t length, const char *file)
{
	char buffer[64];
	size_t n;
	if (!read_text_file(buffer, sizeof(buffer), &n, file))
		return false;

	strncpy(dst, buffer, length);

	if (n > 1)
		dst[strlen(dst)-1] = '\0';

	return true;
}

bool get_pisound_version(char *dst, size_t length)
{
	return read_pisound_system_file(dst, length, PISOUND_VERSION_FILE);
}

bool get_pisound_serial(char *dst, size_t length)
{
	return read_pisound_system_file(dst, length, PISOUND_SERIAL_FILE);
}

bool get_pisound_id(char *dst, size_t length)
{
	return read_pisound_system_file(dst, length, PISOUND_ID_FILE);
}

static void onTimesClicked(unsigned num_presses)
{
	char cmd[64];
	sprintf(cmd, "%s %u", CLICK_ACTION, num_presses);
	system(cmd);
}

static void onDown(void)
{
	system(DOWN_ACTION);
}

static void onUp(void)
{
	system(UP_ACTION);
}

static void onHold(unsigned num_presses, timestamp_ms_t timeHeld)
{
	char cmd[64];
	sprintf(cmd, "%s %u %u", HOLD_ACTION, num_presses, timeHeld);
	system(cmd);
}

int run(void)
{
	char version_string[64];
	char serial[64];
	char id[64];

	if (!get_pisound_serial(serial, sizeof(serial)))
	{
		fprintf(stderr, "Reading pisound serial failed, did the kernel module load successfully?\n");
		return -EINVAL;
	}
	if (!get_pisound_id(id, sizeof(id)))
	{
		fprintf(stderr, "Reading pisound id failed, did the kernel module load successfully?\n");
		return -EINVAL;
	}

	unsigned short version = INVALID_VERSION;
	if (!get_pisound_version(version_string, sizeof(version_string)) || !parse_version(&version, version_string))
	{
		fprintf(stderr, "Reading pisound version failed, did the kernel module load successfully?\n");
		return -EINVAL;
	}

	if (is_update_check_enabled())
		check_for_updates(PISOUND_BTN_VERSION, version_string, serial, id);

	if (version < 0x0100 || version >= 0x0200)
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
						onHold(num_pressed, timestamp - pressed_at);
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
