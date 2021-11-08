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

if [ "$(systemctl is-system-running || true)" = "stopping" ]; then
	flash_leds 1
	log "System is stopping anyway, skipping disabling WiFi hotspot."
	exit
fi

flash_leds 1
log "Disabling Access point..."

sudo systemctl stop wifi-hotspot
sudo systemctl disable wifi-hotspot

flash_leds 20
sleep 0.5
flash_leds 20
