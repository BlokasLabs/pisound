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

SCRIPT_PATH=$(dirname $(readlink -f $0))

. /usr/local/pisound/scripts/common/common.sh

flash_leds 1
log "Enabling Access point..."

sudo rfkill unblock wifi
sudo wpa_cli -i wlan0 disconnect
sudo dhcpcd --denyinterfaces wlan0
sudo ifconfig wlan0 down
sudo ifconfig wlan0 172.24.1.1 netmask 255.255.255.0 broadcast 172.24.1.255
sudo systemctl stop dnsmasq
sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
sudo iptables -A FORWARD -i eth0 -o wlan0 -m state --state RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i wlan0 -o eth0 -j ACCEPT
sudo sh -c "echo 1 > /proc/sys/net/ipv4/ip_forward"
sudo systemctl start dnsmasq
(sleep 15 && sudo systemctl restart avahi-daemon) &
sudo systemctl unmask hostapd
sudo systemctl start hostapd

sudo systemctl restart touchosc2midi 2>/dev/null

flash_leds 20
