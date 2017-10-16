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
. /usr/local/pisound/scripts/common/start_puredata.sh

log "Searching for main.pd in USB storage!"

PURE_DATA_PATCH=`find /media 2> /dev/null | grep main.pd | head -1`

if [ -z "$PURE_DATA_PATCH" ]; then
	log "No patch found in attached media, trying to mount USB devices..."
	for usb_dev in `ls /dev/disk/by-id/ | grep usb`; do
		DISKPATH="/dev/disk/by-id/$usb_dev"
		DEV=$(readlink -f $DISKPATH)

		LABEL=`sudo blkid -s LABEL -o value $DEV`
		if [ -z "$LABEL" ]; then
			log "Skipping $DISKPATH"
			continue
		fi

		MEDIAPATH="/media/$USER/$LABEL"
		log "Mounting $DEV ($LABEL) to $MEDIAPATH"
		sudo mkdir -p "$MEDIAPATH"
		sudo chown $USER "$MEDIAPATH"
		sudo chgrp $USER "$MEDIAPATH"
		sudo mount "$DEV" "$MEDIAPATH"
	done

	PURE_DATA_PATCH=`find /media 2> /dev/null | grep main.pd | head -1`

	if [ -z "$PURE_DATA_PATCH" ]; then
		log "No patch found in attached media, checking /usr/local/puredata-patches..."
		PURE_DATA_PATCH=`find /usr/local/puredata-patches/ 2> /dev/null | grep main.pd | head -1`
	fi
fi

if [ -z "$PURE_DATA_PATCH" ]; then
	log "No patch found! Doing nothing..."
	sleep 0.5
	flash_leds 100
	exit 0
else
	log "Found patch: $PURE_DATA_PATCH"
fi

log "All sanity checks succeeded."

start_puredata "$PURE_DATA_PATCH" &
