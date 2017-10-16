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

log "Pisound button held for $2 ms, after $1 clicks!"

if [ $1 -ne 1 ]; then
	log "Ignoring hold after $1 clicks..."
	exit 0
fi

aconnect -x

for i in $(seq 1 10); do
	flash_leds 1
	sleep 0.1
done

log "Shutting down."

sudo shutdown now
