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

if [ "$1" != "true" ] && [ "$1" != "false" ]; then
	echo Expected 'true' or 'false' argument to be provided! 1>&2
	exit 1
fi

. /usr/local/etc/pisound/common.sh

periodic_led_blink 0 0

PID_FILE=$HOME/.pisound-discoverable-pid

if [ -e $PID_FILE ]; then
	kill `cat $PID_FILE` 2>/dev/null
	rm -f $PID_FILE
fi

TIMEOUT=0
for btdev in `gdbus introspect --system --dest org.bluez --object-path / --recurse | grep hci | grep -e '/hci[0..9]* ' | awk '/^ *node /{print $2}'`; do
	log Setting Discoverable prop of $btdev to $1
	gdbus call --system --dest org.bluez --object-path $btdev --method org.freedesktop.DBus.Properties.Set org.bluez.Adapter1 "Discoverable" "<$1>" > /dev/null;
	TIMEOUT=`gdbus call --system --dest org.bluez --object-path /org/bluez/hci0 --method org.freedesktop.DBus.Properties.Get org.bluez.Adapter1 "DiscoverableTimeout" | awk '{print $2}' | egrep -o [0-9]*`
done

if [ "$1" = "true" ]; then
	(
		(
			periodic_led_blink 15 1 /tmp/.bt-discoverable-blink-pid
			sleep $TIMEOUT
			periodic_led_blink 0 0 /tmp/.bt-discoverable-blink-pid
			rm -f $PID_FILE
		)&
		echo $! > $PID_FILE
	)
else
	periodic_led_blink 0 0 /tmp/.bt-discoverable-blink-pid
fi
