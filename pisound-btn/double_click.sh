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

. $(dirname $(readlink -f $0))/common.sh

log "pisound button double clicked!"
aconnect -x
flash_out_led 1

log "Killing all Pure Data instances!"

for pd in `ps -C puredata --no-headers | awk '{print $1;}'`; do
	log "Killing pid $pd..."
	kill $pd
done

log "Syncing IO!"
sync

log "Unmounting all usb drives!"
for usb_dev in /dev/disk/by-id/usb-*; do
	dev=$(readlink -f $usb_dev)
	grep -q ^$dev /proc/mounts && sudo umount $dev
done

log "Done, flashing led."
flash_out_led 10
sleep 0.5
flash_out_led 10
