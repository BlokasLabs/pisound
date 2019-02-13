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

PURE_DATA_STARTUP_SLEEP=3

. /usr/local/pisound/scripts/common/common.sh

# If there's X server running
if DISPLAY=$(find_display); then
	export XAUTHORITY=/home/pi/.Xauthority
	export DISPLAY
	echo Using display $DISPLAY
	unset NO_GUI
else
	echo No display found, specifying -nogui
	NO_GUI=-nogui
fi

start_puredata()
{
	flash_leds 1

	if [ -z `which puredata` ]; then
		log "Pure Data was not found! Install by running: sudo apt-get install puredata"
		flash_leds 100
		exit 1
	fi

	log "Killing all Pure Data instances!"
	killall puredata 2> /dev/null

	PATCH="$1"
	PATCH_DIR=$(dirname "$PATCH")
	shift

	log "Launching Pure Data."
	cd "$PATCH_DIR" && puredata -stderr $NO_GUI -send ";pd dsp 1" "$PATCH" $@ &
	PD_PID=$!

	log "Pure Data started!"
	flash_leds 1
	sleep 0.3
	flash_leds 1
	sleep 0.3
	flash_leds 1

	wait_process $PD_PID
}
