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
	aconnect -x
	flash_leds 1

	if [ -z `which puredata` ]; then
		log "Pure Data was not found! Install by running: sudo apt-get install puredata"
		flash_leds 100
		exit 1
	fi

	log "Killing all Pure Data instances!"
	killall puredata 2> /dev/null

	(
		log "Giving $PURE_DATA_STARTUP_SLEEP seconds for Pure Data to start up before connecting MIDI ports."
		sleep $PURE_DATA_STARTUP_SLEEP

		READABLE_PORTS=`aconnect -i | egrep -iv "(Through|Pure Data|System)" | egrep -o "[0-9]+:" | egrep -o "[0-9]+"`
		RANGE="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"

		log "Connecting all MIDI ports to and from Pure Data."

		for p in $READABLE_PORTS; do
			for i in $RANGE; do
				aconnect $p:$i "Pure Data" 2> /dev/null;
				aconnect "Pure Data:1" $p:$i 2> /dev/null;
			done;
		done
	) &

	PATCH="$1"
	PATCH_DIR=$(dirname "$PATCH")
	shift

	log "Launching Pure Data."
	cd "$PATCH_DIR" && puredata -stderr -alsa $(/usr/local/pisound/scripts/common/find_pd_alsa_devices.py "pisound (hardware)") -alsamidi -channels 2 -r 48000 $NO_GUI -mididev 1 -send ";pd dsp 1" "$PATCH" $@ &
	PD_PID=$!

	log "Pure Data started!"
	flash_leds 1
	sleep 0.3
	flash_leds 1
	sleep 0.3
	flash_leds 1

	wait_process $PD_PID
}
