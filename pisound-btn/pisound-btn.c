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

#include <stdarg.h>
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
#include <signal.h>
#include <libgen.h>

#define HOMEPAGE_URL "https://blokas.io/pisound/"
#define UPDATE_URL   HOMEPAGE_URL "/updates?btnv=%x.%02x&v=%s&sn=%s&id=%s"

enum { PISOUND_BTN_VERSION     = 0x0112 };
enum { INVALID_VERSION         = 0xffff };
enum { CLICK_TIMEOUT_MS        = 400    };
enum { HOLD_PRESS_TIMEOUT_MS   = CLICK_TIMEOUT_MS };

#define BASE_PISOUND_DIR "/usr/local/pisound"
#define BASE_SCRIPTS_DIR BASE_PISOUND_DIR "/scripts/pisound-btn"

static int g_button_pin = 17;
static bool g_button_exported = false;

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

static const char *const CLICK_NAME="CLICK_%u";
static const char *const CLICK_OTHER_VALUE_NAME    = "CLICK_OTHER";

static const char *const HOLD_NAME="HOLD_%uS";
static const char *const HOLD_OTHER_VALUE_NAME     = "HOLD_OTHER";

static const char *const CLICK_COUNT_LIMIT_VALUE_NAME = "CLICK_COUNT_LIMIT";

static const char *const PISOUND_ID_FILE           = "/sys/kernel/pisound/id";
static const char *const PISOUND_SERIAL_FILE       = "/sys/kernel/pisound/serial";
static const char *const PISOUND_VERSION_FILE      = "/sys/kernel/pisound/version";

static const char *const UPDATE_CHECK_DISABLE_FILE = BASE_PISOUND_DIR "/disable_update_check"; // If the file exists, the update check will be disabled.
static const char *const DEFAULT_DOWN              = BASE_SCRIPTS_DIR "/system/down.sh";
static const char *const DEFAULT_UP                = BASE_SCRIPTS_DIR "/system/up.sh";
static const char *const DEFAULT_CLICK_1           = BASE_SCRIPTS_DIR "/start_puredata.sh";
static const char *const DEFAULT_CLICK_2           = BASE_SCRIPTS_DIR "/stop_puredata.sh";
static const char *const DEFAULT_CLICK_3           = BASE_SCRIPTS_DIR "/toggle_wifi_hotspot.sh";

// Receive 'held after n clicks' and 'time held' arguments.
static const char *const DEFAULT_HOLD_3S           = BASE_SCRIPTS_DIR "/toggle_bt_discoverable.sh";
static const char *const DEFAULT_HOLD_5S           = BASE_SCRIPTS_DIR "/shutdown.sh";

// Arbitrarily chosen limit.
enum { MAX_PATH_LENGTH = 4096 };

static char g_config_path[MAX_PATH_LENGTH+1]  = "/etc/pisound.conf";

// must hold the CLICK_OTHER_VALUE_NAME or HOLD_OTHER_VALUE_NAME values
#define ACTION_NAME_SIZE 11
// printf( CLICK_%u", ABSOLUTE_MAX_CLICK) must fit in ACTION_NAME_SIZE
#define ABSOLUTE_MAX_CLICK 99
// printf( HOLD_%uS", ABSOLUTE_MAX_HOLD) must fit in ACTION_NAME_SIZE
#define ABSOLUTE_MAX_HOLD 99
// convert ticks to seconds so the [0,2) == 1, [2,4) == 3, [4,6) == 5
#define TICK_2_SECONDS(x) (x/1000 + (x%2))


enum { DEFAULT_CLICK_COUNT_LIMIT = 8 };

static unsigned int g_click_count_limit = DEFAULT_CLICK_COUNT_LIMIT;
static unsigned int g_debug = 1;

static void debug( unsigned int level, const char *fmt, ...)
{
	va_list argp;
	if (level <= g_debug) {
		va_start(argp, fmt);
		vfprintf(stderr,fmt,argp);
		va_end(argp);
	}
}


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

static void read_config_value(const char *conf, const char *value_name, char *dst, char *args, size_t n, const char *default_value)
{
	const size_t BUFFER_SIZE = 2 * MAX_PATH_LENGTH + 1;
	char line[BUFFER_SIZE];
	char name[BUFFER_SIZE];
	char argument[BUFFER_SIZE];
	char value[BUFFER_SIZE];

	size_t currentLine = 0;

	bool found = false;

	FILE *f = fopen(conf, "rt");

	while (f && !feof(f))
	{
		if (read_line(f, line, sizeof(line)))
		{
			++currentLine;

			argument[0] = '\0';
			value[0] = '\0';

			char *commentMarker = strchr(line, '#');
			if (commentMarker)
				*commentMarker = '\0'; // Ignore comments.

			char *endline = strchr(line, '\n');
			if (endline)
				*endline = '\0'; // Ignore endline.

			static const char *WHITESPACE_CHARS = " \t";

			// remove leading spaces
			char *p = line + strspn(line, WHITESPACE_CHARS);

			if (strlen(p) > 1)
			{
				char *t = strpbrk(p, WHITESPACE_CHARS);

				// if there is whitespace after the name there may be a value
				if (t)
				{
					strncpy(name, p, t - p);
					name[t - p] = '\0';


					t = t + strspn(t, WHITESPACE_CHARS);

					if (args)
					{
						char *a1 = strpbrk(t, WHITESPACE_CHARS);
						if (a1)
						{
							char *a2 = a1 + strspn(a1, WHITESPACE_CHARS);
							if (a2)
							{
								strcpy(argument, a2);
							}
							*a1 = '\0';
						}
					}

					strcpy(value, t);
				} else {
					strncpy(name, p, BUFFER_SIZE);
				}
				if (strlen(name) != 0)
				{
					if (strcmp(name, value_name) == 0)
					{
						debug( 4, "read_config_value located name %s\n", name );
						size_t lenValue = strlen(value);
						size_t lenArgs = strlen(argument);
						if (lenValue < n && lenArgs < n)
						{
							found = true;

							strncpy(dst, value, lenValue);
							dst[lenValue] = '\0';

							if (args)
							{
								strncpy(args, argument, lenArgs);
								args[lenArgs] = '\0';
							}
							break;
						}
						else
						{
							fprintf(stderr, "Too long value set in %s on line %lu!\n", conf, currentLine);
						}
					}
				}
				else
				{
					fprintf(stderr, "Unexpected syntax in %s on line %lu!\n", conf, currentLine);
				}
			}
		}
	}

	if (f)
		fclose(f);

	if (!found)
	{
		debug( 4, "read_config_value did not find name %s\n", value_name );
		strcpy(dst, default_value);
		if (args)
			*args = '\0';
	}
}

static int get_action_name(enum action_e action, char * action_name, unsigned click_count, unsigned hold_time)
{
	memset( action_name, 0, ACTION_NAME_SIZE+1 );
	switch (action)
	{
	case A_DOWN:
		strcpy( action_name,  DOWN_VALUE_NAME );
		break;
	case A_UP:
		strcpy( action_name,  UP_VALUE_NAME);
		break;
	case A_CLICK:
		strcpy( action_name,  CLICK_NAME);
		if (click_count <= ABSOLUTE_MAX_CLICK)
		{
			sprintf( action_name, CLICK_NAME, click_count );
		} else {
			strcpy( action_name, CLICK_OTHER_VALUE_NAME );
		}
		break;
	case A_HOLD:
		strcpy( action_name, HOLD_NAME);
		unsigned timer = TICK_2_SECONDS(hold_time);
		if (timer <= ABSOLUTE_MAX_HOLD)
		{
			sprintf( action_name, HOLD_NAME, timer );
		} else {
			strcpy( action_name, HOLD_OTHER_VALUE_NAME );
		}
		break;
	default:
		break;
	}
	debug(3, "action %u click count %u hold time %u (%u seconds) yields action name %s\n", action, click_count, hold_time, TICK_2_SECONDS(hold_time), action_name );
	return 0;
}

static void get_action_script(enum action_e action, char* action_name, char *buffer, char* args, unsigned int buff_length)
{
	if (action_name[0] == '\0')
	{
		buffer[0] = '\0';
	}

	read_config_value(g_config_path, action_name, buffer, args, buff_length, "#");
	debug( 2, "read_config_value returned %s\n", buffer );
	// if buffer[0] == '#' then the entry did not exist in the config file.
	if (buffer[0] == '#')// entry was not specified -- use defaults.
	{
		bool found = false;
		switch (action)
		{
		case A_UP:
			strcpy( buffer, DEFAULT_UP );
			if (args)
				args[0] = '\0';
			break;
		case A_DOWN:
			strcpy( buffer, DEFAULT_DOWN );
			if (args)
				args[0] = '\0';
			break;
		case A_CLICK:
			if (strcmp("CLICK_1", action_name) == 0)
			{
				strcpy( buffer, DEFAULT_CLICK_1 );
				if (args)
					args[0] = '\0';
			}
			else if (strcmp("CLICK_2", action_name) == 0)
			{
				strcpy( buffer, DEFAULT_CLICK_2 );
				if (args)
					args[0] = '\0';
			}
			else if (strcmp("CLICK_3", action_name) == 0)
			{
				strcpy( buffer, DEFAULT_CLICK_3 );
				if (args)
					args[0] = '\0';
			}
			else
			{
				read_config_value(g_config_path, CLICK_OTHER_VALUE_NAME, buffer, args, buff_length, "#");
			}
			break;
		case A_HOLD:
			if (strcmp("HOLD_3S", action_name) == 0)
			{
				strcpy( buffer, DEFAULT_HOLD_3S );
				if (args)
					args[0] = '\0';
			}
			else if (strcmp("HOLD_5S", action_name) == 0)
			{
				strcpy( buffer, DEFAULT_HOLD_5S );
				if (args)
					args[0] = '\0';
			} else {
				read_config_value(g_config_path, HOLD_OTHER_VALUE_NAME, buffer, args, buff_length, "#");
			}
			break;
		default:
			break;
		}
	}
	// if there is a # at this point entry existed in the config file but had no value
	if (buffer[0] == '#' ) {
		buffer[0] = '\0';
	}
}

// length of command or an error (negative number).
static int get_action_script_path(enum action_e action, char * action_name, unsigned click_count, unsigned hold_time, char *script, char *args, size_t n)
{
	if (action < 0 || action >= A_COUNT) {
		return -EINVAL;
	}

	get_action_name(action, action_name, click_count, hold_time);
	if (action_name[0] == '\0')
	{
		return -EINVAL;
	}

	char action_script[MAX_PATH_LENGTH + 1];
	char arguments[MAX_PATH_LENGTH + 1];

	get_action_script(action, action_name, action_script, arguments, MAX_PATH_LENGTH );

	debug(1, "script = '%s', args = '%s'\n", action_script, arguments);

	if (action_script[0] == '\0')
	{
		return 0;
	}
	// The path is absolute.
	if (action_script[0] == '/')
	{
		strncpy(script, action_script, n-2);
		script[n-1] = '\0';
	}
	else // The path is relative to the config file location.
	{
		char tmp[MAX_PATH_LENGTH + 1];

		size_t pathLength = strlen(g_config_path);
		strncpy(tmp, g_config_path, sizeof(tmp)-1);
		dirname(tmp);
		strncat(tmp, "/", sizeof(tmp)-1 - strlen(tmp));
		strncat(tmp, action_script, sizeof(tmp)-1 - strlen(tmp));
		tmp[sizeof(tmp)-1] = '\0';

		strncpy(script, tmp, n-1);
		script[n-1] = '\0';
	}

	strncpy(args, arguments, n-1);
	args[n-1] = '\0';

	return strlen(script);
}

static void execute_action(enum action_e action, unsigned click_count, unsigned hold_time)
{
	char cmd[MAX_PATH_LENGTH + 64];
	char arg[MAX_PATH_LENGTH + 64];
	char action_name[ ACTION_NAME_SIZE+1 ];
	int n = get_action_script_path(action, action_name, click_count, hold_time, cmd, arg, sizeof(cmd));
	if (n < 0)
	{
		debug(1, "execute_action: getting script path for action %u click count %u hold time %u (%u seconds) resulted in error %d!\n", action, click_count, hold_time, TICK_2_SECONDS(hold_time), n);
		return;
	}

	if (n == 0)
	{
		debug(1, "execute_action: no command for action %s : click count %u hold time %u (%u seconds)\n", action_name, click_count, hold_time, TICK_2_SECONDS(hold_time));
		return;
	}
	debug(2, "execute_action: action %u click count %u hold time %u (%u seconds)\ncmd %s\nargs %s\n", action, click_count, hold_time, TICK_2_SECONDS(hold_time), cmd, arg);
	char *p = cmd + n;
	size_t remainingSpace = sizeof(cmd) - n + 1;
	int result = 0;

	switch (action)
	{
	case A_CLICK:
		if (strlen(arg) == 0) result = snprintf(p, remainingSpace, " %u", click_count);
		else result = snprintf(p, remainingSpace, " %s", arg);
		break;
	case A_HOLD:
		if (strlen(arg) == 0) result = snprintf(p, remainingSpace, " %u %u", click_count, hold_time);
		else result = snprintf(p, remainingSpace, " %s", arg);
		break;
	default:
		break;
	}

	if (result < 0 || result >= remainingSpace)
	{
		fprintf(stderr, "execute_action: failed setting up arguments for action %u, result: %d!\n", action, result);
		return;
	}
	debug(2, "execute_action: executing %s\n", cmd );
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

// Returns negative value on error, 0 if the pin is already exported, 1 if pin was just exported successfully.
static int gpio_export(int pin)
{
	if (!gpio_is_pin_valid(pin))
	{
		fprintf(stderr, "Invalid pin number %d!\n", pin);
		return -1;
	}

	char gpio[64];

	snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%d", pin);

	struct stat s;
	if (stat(gpio, &s) != 0)
	{
		int fd = open("/sys/class/gpio/export", O_WRONLY);
		if (fd == -1)
		{
			fprintf(stderr, "Failed top open /sys/class/gpio/export!\n");
			return -1;
		}
		char str_pin[4];
		snprintf(str_pin, 3, "%d", pin);
		str_pin[3] = '\0';
		const int n = strlen(str_pin)+1;
		int result = write(fd, str_pin, n);
		if (result != n)
		{
			fprintf(stderr, "Failed writing to /sys/class/gpio/export! Error %d.\n",  errno);
			close(fd);
			return -1;
		}
		result = close(fd);
		if (result != 0)
		{
			fprintf(stderr, "Failed closing /sys/class/gpio/export! Error %d.\n", errno);
			return -1;
		}
		// Give some time for the pin to appear.
		usleep(100000);
		return 1;
	}

	// Already exported.
	return 0;
}

static int gpio_unexport(int pin)
{
	if (!gpio_is_pin_valid(pin))
	{
		fprintf(stderr, "Invalid pin number %d!\n", pin);
		return -1;
	}

	char gpio[64];

	snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%d", pin);

	struct stat s;
	if (stat(gpio, &s) == 0)
	{
		int fd = open("/sys/class/gpio/unexport", O_WRONLY);
		if (fd == -1)
		{
			fprintf(stderr, "Failed top open /sys/class/gpio/unexport!\n");
			return -1;
		}
		char str_pin[4];
		snprintf(str_pin, 3, "%d", pin);
		str_pin[3] = '\0';
		const int n = strlen(str_pin)+1;
		int result = write(fd, str_pin, n);
		if (result != n)
		{
			fprintf(stderr, "Failed writing to /sys/class/gpio/unexport! Error %d.\n",  errno);
			close(fd);
			return -1;
		}
		result = close(fd);
		if (result != 0)
		{
			fprintf(stderr, "Failed closing /sys/class/gpio/unexport! Error %d.\n", errno);
			return -1;
		}
		return 0;
	}

	// Already unexported.
	return 0;
}

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

static void onDown()
{
	execute_action(A_DOWN, 0, 0);
}

static void onUp()
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
		strcpy(serial, "NO_PISOUND");
		fprintf(stderr, "Reading Pisound serial failed, Pisound is possibly not attached. Continuing anyway.\n");
	}
	if (!get_pisound_id(id, sizeof(id)))
	{
		strcpy(id, "NO_PISOUND");
		fprintf(stderr, "Reading Pisound id failed, Pisound is possibly not attached. Continuing anyway.\n");
	}

	unsigned short version = INVALID_VERSION;
	if (!get_pisound_version(version_string, sizeof(version_string)) || !parse_version(&version, version_string))
	{
		fprintf(stderr, "Reading Pisound version failed, Pisound is possibly not attached. Continuing anyway.\n");
	}

	if (is_update_check_enabled())
		check_for_updates(PISOUND_BTN_VERSION, version_string, serial, id);

	if ((version < 0x0100 || version >= 0x0200) && version != INVALID_VERSION)
	{
		fprintf(stderr, "The kernel module version (%04x) and pisound-btn version (%04x) are incompatible! Please check for updates at " HOMEPAGE_URL "\n", version, PISOUND_BTN_VERSION);
		return -EINVAL;
	}

	int err = gpio_export(g_button_pin);

	if (err < 0) return err;
	else g_button_exported = (err == 1);

	err = gpio_set_edge(g_button_pin, E_BOTH);

	if (err != 0)
		return err;

	enum
	{
		FD_BUTTON = 0,
		FD_TIMER  = 1,
		FD_COUNT
	};

	int btnfd = gpio_open(g_button_pin);
	if (btnfd == -1)
		return errno;

	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timerfd == -1)
	{
		fprintf(stderr, "Creating timer failed. Error %d.\n", errno);
		gpio_close(btnfd);
		return errno;
	}

	printf("Listening to events on GPIO #%d\n", g_button_pin);

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
					if (g_click_count_limit == 0 || num_pressed < g_click_count_limit)
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

	gpio_set_edge(g_button_pin, E_NONE);
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
		"\t--help               Display the usage information.\n"
		"\t--version            Show the version information.\n"
		"\t--gpio               The pin GPIO number to use for the button. Default is 17.\n"
		"\t--conf               Specify the path to configuration file to use. Default is /etc/pisound.conf.\n"
		"\t--press-count-limit  Set the press count limit. Use 0 for no limit. Default is 8.\n"
		"\t--debug <n>          Enable debugging at level n (higher value = more logging)"
		"\t-n                   Short for --click-count-limit.\n"
		"\t-q                   Short for --debug 0 (turns off all but errors)\n"
		"\n"
		);
	print_version();
}

static bool parse_uint(unsigned int *dst, const char *src)
{
	char * endPtr;
	uint32_t x = strtoul(src, &endPtr, 10);
	if (endPtr == src || *endPtr != '\0')
	{
		*dst = 0;
		return false;
	}
	*dst = x;
	return true;
}

static bool read_config_uint(const char *conf, const char *value_name, unsigned int *dst, unsigned int default_value)
{
	char buffer[12];
	read_config_value(conf, CLICK_COUNT_LIMIT_VALUE_NAME, buffer, NULL, sizeof(buffer), "#");
	if (buffer[0] == '#') // No value was specified.
	{
		*dst = default_value;
		return false;
	}

	if (parse_uint(dst, buffer))
	{
		return true;
	}
	else
	{
		*dst = default_value;
		return false;
	}
}

static void cleanup(void)
{
	if (g_button_exported)
	{
		gpio_unexport(g_button_pin);
		g_button_exported = false;
	}
}

static void sigint_handler(int signum)
{
	exit(0);
}

int main(int argc, char **argv, char **envp)
{
	atexit(&cleanup);
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = &sigint_handler;
	sigaction(SIGINT, &action, NULL);

	int i;
	bool conf_path_specified = false;
	bool click_count_limit_specified = false;
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
		else if (strcmp(argv[i], "--debug") == 0)
		{
			if (i + 1 < argc )
			{
				unsigned int x;
				if (parse_uint(&x, argv[i+1]))
				{
					g_debug = x;
					++i;
				}
			}
			else
			{
				printf("Missing numeric argument for '%s'!\n", argv[i]);
				print_usage();
				return 1;
			}
		}
		else if (strcmp(argv[i], "-q") == 0)
		{
			g_debug = 0;
		}
		else if (strcmp(argv[i], "--conf") == 0)
		{
			if (i + 1 < argc)
			{
				strncpy(g_config_path, argv[i+1], MAX_PATH_LENGTH);
				g_config_path[MAX_PATH_LENGTH] = '\0';
				conf_path_specified = true;
				++i;
			}
			else
			{
				printf("Missing path argument for '%s'!\n", argv[i]);
				print_usage();
				return 1;
			}
		}
		else if (strcmp(argv[i], "--click-count-limit") == 0 || strcmp(argv[i], "-n") == 0)
		{
			if (i + 1 < argc)
			{
				unsigned int x;
				if (parse_uint(&x, argv[i+1]))
				{
					g_click_count_limit = x;
					click_count_limit_specified = true;
					++i;
				}
				else
				{
					printf("Failed parsing count argument for '%s'!\n", argv[i]);
					print_usage();
					return 1;
				}
			}
			else
			{
				printf("Missing count argument for '%s'!\n", argv[i]);
				print_usage();
				return 1;
			}
		}
		else if (strcmp(argv[i], "--gpio") == 0)
		{
			if (i + 1 < argc)
			{
				unsigned int x;
				if (parse_uint(&x, argv[i+1]))
				{
					g_button_pin = (int)x;
					++i;
				}
				else
				{
					printf("Failed parsing GPIO argument for '%s'!\n", argv[i]);
					print_usage();
					return 1;
				}
			}
			else
			{
				printf("Missing GPIO argument for '%s'!\n", argv[i]);
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

	if (!conf_path_specified)
	{
		for (i=0; envp[i] != NULL; ++i)
		{
			if (strncmp("PISOUND_BTN_CFG=", envp[i], 16) == 0)
			{
				const char *cfg = &envp[i][16];
				strncpy(g_config_path, cfg, sizeof(g_config_path)-1);
				g_config_path[sizeof(g_config_path)-1] = '\0';
			}
		}
	}

	if (!click_count_limit_specified)
	{
		read_config_uint(g_config_path, CLICK_COUNT_LIMIT_VALUE_NAME, &g_click_count_limit, g_click_count_limit);
	}

	return run();
}
