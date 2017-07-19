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

SCRIPT_PATH=$(dirname $(readlink -f $0))

. $SCRIPT_PATH/common.sh

log "pisound button triple clicked!"
flash_leds 1

if ps -e | grep -q hostapd; then
	log "Disabling Access point..."
	sh $SCRIPT_PATH/disable_access_point.sh
	killall touchosc2midi

	flash_leds 20
	sleep 0.5
	flash_leds 20
else
	log "Enabling Access point..."
	sh $SCRIPT_PATH/enable_access_point.sh
	if which touchosc2midi; then
		nohup touchosc2midi --ip=172.24.1.1 > /dev/null 2>&1 &
	fi

	flash_leds 20
fi
