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

WAS_ON=false

for btdev in `gdbus introspect --system --dest org.bluez --object-path / --recurse | grep hci | grep -e '/hci[0..9]* ' | awk '/^ *node /{print $2}'`; do
	if [ "`gdbus call --system --dest org.bluez --object-path /org/bluez/hci0 --method org.freedesktop.DBus.Properties.Get org.bluez.Adapter1 'Discoverable'`" = "(<true>,)" ]; then
		WAS_ON=true
		break
	fi
done

if [ $WAS_ON = true ]; then
	/usr/local/pisound/scripts/pisound-btn/system/set_bt_discoverable.sh false
else
	/usr/local/pisound/scripts/pisound-btn/system/set_bt_discoverable.sh true
fi
