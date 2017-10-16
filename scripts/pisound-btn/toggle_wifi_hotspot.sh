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

. /usr/local/pisound/scripts/common/common.sh

log "Pisound button triple clicked!"
flash_leds 1

if ps -e | grep -q hostapd; then
	log "Disabling Access point..."
	sh /usr/local/pisound/scripts/pisound-btn/disable_wifi_hotspot.sh
	killall touchosc2midi

	flash_leds 20
	sleep 0.5
	flash_leds 20
else
	log "Enabling Access point..."
	sh /usr/local/pisound/scripts/pisound-btn/enable_wifi_hotspot.sh
	if which touchosc2midi; then
		nohup touchosc2midi --ip=172.24.1.1 > /dev/null 2>&1 &
	fi

	flash_leds 20
fi
