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

BINARY_DIR ?= /usr/local/bin
SCRIPTS_DIR ?= /usr/local/etc/pisound
PATCHES_DIR ?= /usr/local/etc/pisound-patches
BACKUP_DIR := $(shell echo $(SCRIPTS_DIR)/backups/`date +"%F.%T"`)

all: pisound-btn

pisound-btn: pisound-btn.c
	gcc pisound-btn.c -o pisound-btn

backup-scripts:
	@echo "Backing up scripts in $(SCRIPTS_DIR) to $(BACKUP_DIR)"
	@mkdir -p $(BACKUP_DIR)
	@if test -e $(SCRIPTS_DIR)/click.sh; then \
		cp -pR $(SCRIPTS_DIR)/click.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/single_click.sh; then \
		cp -pR $(SCRIPTS_DIR)/single_click.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/double_click.sh; then \
		cp -pR $(SCRIPTS_DIR)/double_click.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/down.sh; then \
		cp -pR $(SCRIPTS_DIR)/down.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/up.sh; then \
		cp -pR $(SCRIPTS_DIR)/up.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/hold.sh; then \
		cp -pR $(SCRIPTS_DIR)/hold.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/common.sh; then \
		cp -pR $(SCRIPTS_DIR)/common.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/tripple_click.sh; then \
		cp -pR $(SCRIPTS_DIR)/tripple_click.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/enable_access_point.sh; then \
		cp -pR $(SCRIPTS_DIR)/enable_access_point.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/disable_access_point.sh; then \
		cp -pR $(SCRIPTS_DIR)/disable_access_point.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/hostapd.conf; then \
		cp -pR $(SCRIPTS_DIR)/hostapd.conf $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/dnsmasq.conf; then \
		cp -pR $(SCRIPTS_DIR)/dnsmasq.conf $(BACKUP_DIR)/; \
	fi

install: all backup-scripts
	@if test -e /etc/init.d/pisound-btn; then \
		update-rc.d pisound-btn remove; \
		/etc/init.d/pisound-btn stop; \
	fi
	@killall -PIPE pisound-btn > /dev/null 2>&1 || true
	@cp pisound-btn $(BINARY_DIR)/
	@mkdir -p $(PATCHES_DIR)
	@chmod 777 $(PATCHES_DIR)
	@cp -p single_click.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/single_click.sh
	@cp -p double_click.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/double_click.sh
	@cp -p tripple_click.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/tripple_click.sh
	@cp -p up.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/up.sh
	@cp -p down.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/down.sh
	@cp -p hold.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/hold.sh
	@cp -p common.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/common.sh
	@cp -p enable_access_point.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/enable_access_point.sh
	@cp -p disable_access_point.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/disable_access_point.sh
	@cp -p dnsmasq.conf $(SCRIPTS_DIR)/
	@cp -p hostapd.conf $(SCRIPTS_DIR)/
	@cp -p pisound-btn-daemon.desktop /etc/xdg/autostart/

clean:
	rm -f pisound-btn

.PHONY: backup-scripts
