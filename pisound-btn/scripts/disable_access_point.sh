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

sudo dhcpcd --allowinterfaces wlan0
sudo killall hostapd
sudo killall dnsmasq
sudo ifconfig wlan0 0.0.0.0
sudo sh -c "echo | iptables-restore"
sudo sh -c "echo 0 > /proc/sys/net/ipv4/ip_forward"
sudo iwlist wlan0 scan > /dev/null 2>&1
sudo ifup wlan0
