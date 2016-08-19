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
	@if test -e $(SCRIPTS_DIR)/hold.sh; then \
		cp -pR $(SCRIPTS_DIR)/hold.sh $(BACKUP_DIR)/; \
	fi
	@if test -e $(SCRIPTS_DIR)/common.sh; then \
		cp -pR $(SCRIPTS_DIR)/common.sh $(BACKUP_DIR)/; \
	fi

install: all backup-scripts
	@if test -e /etc/init.d/pisound-btn; then \
		update-rc.d pisound-btn remove; \
		/etc/init.d/pisound-btn stop; \
	fi
	@killall pisound-btn 2>/dev/null || true
	@cp pisound-btn $(BINARY_DIR)/
	@cp -p click.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/click.sh
	@cp -p hold.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/hold.sh
	@cp -p common.sh $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/common.sh
	@cp -p pisound-btn-daemon.desktop /etc/xdg/autostart/

clean:
	rm -f pisound-btn

.PHONY: backup-scripts
