#!/bin/sh

# pisound-btn daemon for the pisound button.
# Copyright (C) 2016  Vilniaus Blokas UAB, http://blokas.io/pisound
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
	log "pisound MIDI device not found!"
	exit 0
#else
#	log "pisound MIDI device: $PISOUND_MIDI_DEVICE"
fi

PISOUND_LED_FILE="/sys/kernel/pisound/led"

# Takes an unsigned integer value, [0;255] for flash duration.
flash_leds() {
	if [ -e $PISOUND_LED_FILE ]; then
		sudo sh -c "echo $1 > $PISOUND_LED_FILE"
	else
		amidi -S "f0 f7" -p $PISOUND_MIDI_DEVICE 2> /dev/null
	fi
}
