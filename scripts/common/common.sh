#!/bin/sh

# pisound-btn daemon for the Pisound button.
# Copyright (C) 2017  Vilniaus Blokas UAB, https://blokas.io/pisound
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of the
# License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

log() {
	echo `date +"%F.%T"`: $*
}

PISOUND_MIDI_DEVICE=`amidi -l | grep pisound | egrep -o hw:[0-9]+,[0-9]+`

if [ -z $PISOUND_MIDI_DEVICE ]; then
	log "Pisound MIDI device not found!"
#else
#	log "Pisound MIDI device: $PISOUND_MIDI_DEVICE"
fi

PISOUND_LED_FILE="/sys/kernel/pisound/led"

# Takes an unsigned integer value, [0;255] for flash duration.
if [ -e $PISOUND_LED_FILE ]; then
	flash_leds() {
		sudo sh -c "echo $1 > $PISOUND_LED_FILE"
	}
elif [ ! -z $PISOUND_MIDI_DEVICE ]; then
	flash_leds() {
		amidi -S "f0 f7" -p $PISOUND_MIDI_DEVICE 2> /dev/null
	}
else
	flash_leds() {
		log "Blink."
	}
fi

# Takes [0;255] for flash duration and a float for period. If period is 0, blinking is stopped.
# A third optional argument can be provided - a temporary file for inner process id storage.
# It allows starting multiple 'blink' processes in parallel, and controlling them individually.
# Example value - /tmp/.example-blink-pid
periodic_led_blink() {
	local PID_FILE
	if [ "$#" -eq 3 ]; then
		PID_FILE=$3
	else
		PID_FILE=/tmp/.pisound-blink-pid
	fi

	if [ "$#" -eq 2 ] || [ "$#" -eq 3 ]; then
		if [ "$2" = "0" ]; then
			if [ -e $PID_FILE ]; then
				kill `cat $PID_FILE` 2>/dev/null
				rm -f $PID_FILE
			fi
		else
			if [ -e $PID_FILE ]; then
				# Stop blinking before reconfiguring.
				periodic_led_blink 0 0 $3
			fi
			(
				(while true; sleep $2; do flash_leds $1; done) &
				echo $! > $PID_FILE
			)
		fi
	else
		echo 2 or 3 arguments must be specified! 1>&2
	fi
}

# Waits until the given process terminates.
wait_process() {
	if [ "$#" -ne 1 ]; then
		echo 1 argument must be specified! 1>&2
		return
	fi
	while true; do
		wait $1
		kill -0 $1 2> /dev/null || break
	done
}

# Find first X display.
find_display() {
	# If HDMI display is connected, try default :0 manually.
	if [ "$(tvservice -n 2>&1)" != "[E] No device present" ] && XAUTHORITY=/home/pi/.Xauthority DISPLAY=:0 xhost > /dev/null 2>&1; then
		echo :0
		return 0
	fi

	# HDMI display was unavailable, try finding another display (such as vnc) to use.
	display=$(ps ea | grep -Po "DISPLAY=[\.0-9A-Za-z:]* " | sort -u | head -n 1 | grep -oe :.*)
	if [ -z $display ]; then
		if [ -z $PISOUND_DISPLAY ]; then
			# Not found.
			return 1
		else
			# Fallback to value set in PISOUND_DISPLAY
			echo $PISOUND_DISPLAY
			return 0
		fi
	else
		# Found something.
		echo $display
		return 0
	fi
}
