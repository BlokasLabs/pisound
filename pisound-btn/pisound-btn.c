/*
 * pisound-btn daemon for the Pisound button.
 * Copyright (C) 2017  Vilniaus Blokas UAB, https://blokas.io/pisound
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
#include <assert.h>

#define HOMEPAGE_URL "https://blokas.io/pisound"
#define UPDATE_URL   HOMEPAGE_URL "/updates?btnv=%x.%02x&v=%s&sn=%s&id=%s"

enum { PISOUND_BTN_VERSION     = 0x0104 };
enum { INVALID_VERSION         = 0xffff };
enum { BUTTON_PIN              = 17     };
enum { CLICK_TIMEOUT_MS        = 400    };
enum { HOLD_PRESS_TIMEOUT_MS   = CLICK_TIMEOUT_MS };
enum { PRESS_COUNT_LIMIT       = 8      };

#define BASE_SCRIPTS_DIR "/usr/local/etc/pisound"

enum action_e
{
	A_DOWN = 0, // Executed every time the button is pushed down.
	A_UP,       // Executed every time the button is released up.
	A_CLICK,    // Executed when the button is short-clicked one or multiple times in quick succession.
	A_HOLD,     // Executed if the button was held for given time.

	// Must be the last one!
	A_COUNT
};

static const char *const DOWN_VALUE_NAME           = "DOWN";
static const char *const UP_VALUE_NAME             = "UP";

static const char *const SINGLE_CLICK_VALUE_NAME   = "SINGLE_CLICK";
static const char *const DOUBLE_CLICK_VALUE_NAME   = "DOUBLE_CLICK";
static const char *const TRIPLE_CLICK_VALUE_NAME   = "TRIPLE_CLICK";
static const char *const OTHER_CLICKS_VALUE_NAME   = "OTHER_CLICKS";

static const char *const HOLD_1S_VALUE_NAME        = "HOLD_1S";
static const char *const HOLD_3S_VALUE_NAME        = "HOLD_3S";
static const char *const HOLD_5S_VALUE_NAME        = "HOLD_5S";
static const char *const HOLD_OTHER_VALUE_NAME     = "HOLD_OTHER";

static const char *const PISOUND_ID_FILE           = "/sys/kernel/pisound/id";
static const char *const PISOUND_SERIAL_FILE       = "/sys/kernel/pisound/serial";
static const char *const PISOUND_VERSION_FILE      = "/sys/kernel/pisound/version";

static const char *const UPDATE_CHECK_DISABLE_FILE = BASE_SCRIPTS_DIR "/disable_update_check"; // If the file exists, the update check will be disabled.

static const char *const DEFAULT_DOWN              = BASE_SCRIPTS_DIR "/down.sh";
static const char *const DEFAULT_UP                = BASE_SCRIPTS_DIR "/up.sh";

static const char *const DEFAULT_SINGLE_CLICK      = BASE_SCRIPTS_DIR "/start_puredata.sh";
static const char *const DEFAULT_DOUBLE_CLICK      = BASE_SCRIPTS_DIR "/stop_puredata.sh";
static const char *const DEFAULT_TRIPLE_CLICK      = BASE_SCRIPTS_DIR "/toggle_wifi_hotspot.sh";

// Receives 'times clicked' argument.
static const char *const DEFAULT_OTHER_CLICKS      = BASE_SCRIPTS_DIR "/click.sh";

// Receive 'held after n clicks' and 'time held' arguments.
static const char *const DEFAULT_HOLD_1S           = BASE_SCRIPTS_DIR "/shutdown.sh";
static const char *const DEFAULT_HOLD_3S           = BASE_SCRIPTS_DIR "/shutdown.sh";
static const char *const DEFAULT_HOLD_5S           = BASE_SCRIPTS_DIR "/shutdown.sh";
static const char *const DEFAULT_HOLD_OTHER        = BASE_SCRIPTS_DIR "/shutdown.sh";

// Arbitrarily chosen limit.
enum { MAX_PATH_LENGTH = 4096 };

static char g_config_path[MAX_PATH_LENGTH+1]  = "/etc/pisound.conf";

// Reads a line, truncates it if needed, seeks to the next line.
static bool read_line(FILE *f, char *buffer, size_t n)
{
	memset(buffer, 0, n);

	if (fgets(buffer, n, f) == NULL)
		return false;

	if (buffer[n-2] != '\0' && buffer[n-2] != '\n')
	{
		// Whole line didn't fit into the buffer, seek until next line.
		while (!feof(f))
		{
			int c = fgetc(f);
			if (c == '\n')
				break;
		}
	}

	return true;
}

static void read_config_value(const char *conf, const char *value_name, char *dst, size_t n, const char *default_value)
{
	FILE *f = fopen(conf, "rt");
	if (f == NULL)
		return;

	const size_t BUFFER_SIZE = 2 * MAX_PATH_LENGTH + 1;
	char line[BUFFER_SIZE];
	char name[BUFFER_SIZE];
	char value[BUFFER_SIZE];

	size_t currentLine = 0;

	bool found = false;

	while (!feof(f))
	{
		if (read_line(f, line, sizeof(line)))
		{
			++currentLine;

			if (strlen(line) > 1)
			{
				int count = sscanf(line, "%s %s", name, value);
				if (count == 2)
				{
					// Skip comments.
					if (name[0] == '#')
						continue;

					if (strcmp(name, value_name) == 0)
					{
						size_t len = strlen(value);
						if (len < n)
						{
							strncpy(dst, value, len);
							dst[len] = '\0';
							found = true;
						}
						else
						{
							fprintf(stderr, "Too long value set in %s on line %u!\n", conf, currentLine);
						}
					}
				}
				else
				{
					if (count != 1 || name[0] != '#')
					{
						fprintf(stderr, "Unexpected syntax in %s on line %u!\n", conf, currentLine);
					}
				}
			}
		}
	}

	fclose(f);

	if (!found)
	{
		strcpy(dst, default_value);
	}
}

static void get_default_action_and_script(enum action_e action, unsigned arg0, unsigned arg1, const char **name, const char **script)
{
	switch (action)
	{
	case A_DOWN:
		*name = DOWN_VALUE_NAME;
		*script = DEFAULT_DOWN;
		break;
	case A_UP:
		*name = UP_VALUE_NAME;
		*script = DEFAULT_UP;
		break;
	case A_CLICK:
		switch (arg0)
		{
		case 1:
			*name = SINGLE_CLICK_VALUE_NAME;
			*script = DEFAULT_SINGLE_CLICK;
			break;
		case 2:
			*name = DOUBLE_CLICK_VALUE_NAME;
			*script = DEFAULT_DOUBLE_CLICK;
			break;
		case 3:
			*name = TRIPLE_CLICK_VALUE_NAME;
			*script = DEFAULT_TRIPLE_CLICK;
			break;
		default:
			*name = OTHER_CLICKS_VALUE_NAME;
			*script = DEFAULT_OTHER_CLICKS;
			break;
		}
		break;
	case A_HOLD:
		if (arg1 < 3000)
		{
			*name = HOLD_1S_VALUE_NAME;
			*script = DEFAULT_HOLD_1S;
		}
		else if (arg1 < 5000)
		{
			*name = HOLD_3S_VALUE_NAME;
			*script = DEFAULT_HOLD_3S;
		}
		else if (arg1 < 7000)
		{
			*name = HOLD_5S_VALUE_NAME;
			*script = DEFAULT_HOLD_5S;
		}
		else
		{
			*name = HOLD_OTHER_VALUE_NAME;
			*script = DEFAULT_HOLD_OTHER;
		}
		break;
	default:
		*name = NULL;
		*script = NULL;
		break;
	}
}

// Returns the length of the path or an error (negative number).
static int get_action_script_path(enum action_e action, unsigned arg0, unsigned arg1, char *dst, size_t n)
{
	if (action < 0 || action >= A_COUNT)
		return -EINVAL;

	const char *action_name = NULL;
	const char *default_script = NULL;

	get_default_action_and_script(action, arg0, arg1, &action_name, &default_script);

	if (action_name == NULL || default_script == NULL)
		return -EINVAL;

	char script[MAX_PATH_LENGTH + 1];

	read_config_value(g_config_path, action_name, dst, n, default_script);

	return strlen(dst);
}

static void execute_action(enum action_e action, unsigned arg0, unsigned arg1)
{
	char cmd[MAX_PATH_LENGTH + 64];
	int n = get_action_script_path(action, arg0, arg1, cmd, sizeof(cmd));
	if (n < 0)
	{
		fprintf(stderr, "execute_action: getting script path for action %u resulted in error %d!\n", action, n);
		return;
	}

	char *p = cmd + n;
	size_t remainingSpace = sizeof(cmd) - n + 1;
	int result = 0;

	switch (action)
	{
	case A_CLICK:
		result = snprintf(p, remainingSpace, " %u", arg0);
		break;
	case A_HOLD:
		result = snprintf(p, remainingSpace, " %u %u", arg0, arg1);
		break;
	default:
		break;
	}

	if (result < 0 || result >= remainingSpace)
	{
		fprintf(stderr, "execute_action: failed setting up arguments for action %u, result: %d!\n", action, result);
		return;
	}

	system(cmd);
}

static int gpio_is_pin_valid(int pin)
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

static int gpio_set_edge(int pin, enum edge_e edge)
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

static int gpio_open(int pin)
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

static int gpio_close(int fd)
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

static timestamp_ms_t get_timestamp_ms(void)
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

static bool get_pisound_version(char *dst, size_t length)
{
	return read_pisound_system_file(dst, length, PISOUND_VERSION_FILE);
}

static bool get_pisound_serial(char *dst, size_t length)
{
	return read_pisound_system_file(dst, length, PISOUND_SERIAL_FILE);
}

static bool get_pisound_id(char *dst, size_t length)
{
	return read_pisound_system_file(dst, length, PISOUND_ID_FILE);
}

static void onTimesClicked(unsigned num_presses)
{
	execute_action(A_CLICK, num_presses, 0);
}

static void onDown(void)
{
	execute_action(A_DOWN, 0, 0);
}

static void onUp(void)
{
	execute_action(A_UP, 0, 0);
}

static void onHold(unsigned num_presses, timestamp_ms_t time_held)
{
	execute_action(A_HOLD, num_presses, time_held);
}

static int run(void)
{
	char version_string[64];
	char serial[64];
	char id[64];

	if (!get_pisound_serial(serial, sizeof(serial)))
	{
		fprintf(stderr, "Reading Pisound serial failed, did the kernel module load successfully?\n");
		return -EINVAL;
	}
	if (!get_pisound_id(id, sizeof(id)))
	{
		fprintf(stderr, "Reading Pisound id failed, did the kernel module load successfully?\n");
		return -EINVAL;
	}

	unsigned short version = INVALID_VERSION;
	if (!get_pisound_version(version_string, sizeof(version_string)) || !parse_version(&version, version_string))
	{
		fprintf(stderr, "Reading Pisound version failed, did the kernel module load successfully?\n");
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

static void print_version(void)
{
	printf("Version %x.%02x, Blokas Labs " HOMEPAGE_URL "\n", PISOUND_BTN_VERSION >> 8, PISOUND_BTN_VERSION & 0xff);
}

static void print_usage(void)
{
	printf("Usage: pisound-btn [options]\n"
		"Options:\n"
		"\t--help      Display the usage information.\n"
		"\t--version   Show the version information.\n"
		"\t--conf      Specify the path to configuration file to use. Default is /etc/pisound.conf.\n"
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
		else if (strcmp(argv[i], "--conf") == 0)
		{
			if (i + 1 < argc)
			{
				strncpy(g_config_path, argv[i+1], MAX_PATH_LENGTH);
				g_config_path[MAX_PATH_LENGTH] = '\0';
				++i;
			}
			else
			{
				printf("Missing path argument for '%s'!\n", argv[i]);
				print_usage();
				return 1;
			}
		}
		else
		{
			printf("Unknown option '%s'.\n", argv[i]);
			print_usage();
			return 1;
		}
	}

	return run();
}
