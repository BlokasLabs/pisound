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

CURRENT_SCRIPT_DIR=$(dirname $(readlink -f $0))

. $CURRENT_SCRIPT_DIR/common.sh

log "pisound button clicked $1 times!"

if [ $1 -eq 1 ]; then
	$CURRENT_SCRIPT_DIR/single_click.sh
elif [ $1 -eq 2 ]; then
	$CURRENT_SCRIPT_DIR/double_click.sh
elif [ $1 -eq 3 ]; then
	$CURRENT_SCRIPT_DIR/triple_click.sh
else
	log "No action for $1 clicks"
fi
