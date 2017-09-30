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

BINARY_DIR ?= /usr/local/bin
SCRIPTS_DIR ?= /usr/local/etc/pisound
PATCHES_DIR ?= /usr/local/etc/pisound-patches
CURRENT_DATE := $(shell date +"%F.%T")
BACKUP_DIR ?= /usr/local/etc/pisound-backups/$(CURRENT_DATE)

all: pisound-btn

pisound-btn: pisound-btn.c
	gcc pisound-btn.c -o pisound-btn

backup-scripts:
	@if test -e $(SCRIPTS_DIR); then \
		echo "Backing up scripts in $(SCRIPTS_DIR) to $(BACKUP_DIR)"; \
		mkdir -p $(BACKUP_DIR); \
		if test -e $(SCRIPTS_DIR)/backups; then \
			echo "Migrating $(SCRIPTS_DIR)/backups to $(BACKUP_DIR)"; \
			mv $(SCRIPTS_DIR)/backups/* $(BACKUP_DIR)/; \
			rm -r $(SCRIPTS_DIR)/backups/; \
		fi; \
		mv $(SCRIPTS_DIR)/* $(BACKUP_DIR)/; \
		cp /etc/pisound.conf $(BACKUP_DIR)/; \
	fi

install: all backup-scripts
	@if test -e /etc/init.d/pisound-btn; then \
		update-rc.d pisound-btn remove; \
		/etc/init.d/pisound-btn stop; \
	fi
	@killall -PIPE pisound-btn > /dev/null 2>&1 || true
	@systemctl stop pisound-btn > /dev/null 2>&1 || true
	@cp pisound-btn $(BINARY_DIR)/
	@mkdir -p $(SCRIPTS_DIR)
	@mkdir -p $(PATCHES_DIR)
	@chmod 777 $(PATCHES_DIR)
	@cp -pr scripts/*  $(SCRIPTS_DIR)/ && chmod +x $(SCRIPTS_DIR)/*.sh
	@cp -p pisound-btn.service /etc/systemd/system/
	@cp -p pisound.conf /etc/
	@systemctl daemon-reload > /dev/null 2>&1
	@systemctl enable pisound-btn > /dev/null 2>&1
	@systemctl start pisound-btn > /dev/null 2>&1

clean:
	rm -f pisound-btn

.PHONY: backup-scripts
