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
PISOUND_DIR ?= /usr/local/pisound
PATCHES_DIR ?= /usr/local/pisound-patches

all: pisound-btn

pisound-btn: pisound-btn.c
	gcc-4.9 pisound-btn.c -o pisound-btn
	strip pisound-btn

$(PISOUND_DIR):
	@if [ ! -d $(PISOUND_DIR) ]; then \
		echo 'Copying this git repository to $(PISOUND_DIR)'; \
		mkdir -p $(PISOUND_DIR); \
		cp -r ../. $(PISOUND_DIR); \
	fi

install: all $(PISOUND_DIR) # backup-scripts
	@if test -e /etc/init.d/pisound-btn; then \
		update-rc.d pisound-btn remove; \
		/etc/init.d/pisound-btn stop; \
	fi
	@killall -PIPE pisound-btn > /dev/null 2>&1 || true
	@systemctl stop pisound-btn > /dev/null 2>&1 || true
	@cp pisound-btn $(BINARY_DIR)/
	@mkdir -p $(PATCHES_DIR)
	@chmod 777 $(PATCHES_DIR)
	@cp -p pisound-btn.service /usr/lib/systemd/system/
	@mkdir -p /usr/local/etc
	@cp -p pisound.conf /usr/local/etc/
	@if test -e /etc/pisound.conf || test -h /etc/pisound.conf; then \
		unlink /etc/pisound.conf || rm /etc/pisound.conf; \
	fi
	@ln -s /usr/local/etc/pisound.conf /etc/pisound.conf
	@systemctl daemon-reload > /dev/null 2>&1
	@systemctl enable pisound-btn > /dev/null 2>&1
	@systemctl start pisound-btn > /dev/null 2>&1

clean:
	rm -f pisound-btn pisound-btn.deb
	rm -rf debian/usr/bin/pisound-btn
	gunzip `find . | grep gz` > /dev/null 2>&1 || true

pisound-btn.deb: pisound-btn
	@gzip --best -n ./debian/usr/share/doc/pisound-btn/changelog ./debian/usr/share/doc/pisound-btn/changelog.Debian ./debian/usr/share/man/man1/pisound-btn.1
	@mkdir -p debian/usr/bin
	@cp pisound-btn debian/usr/bin/
	@mkdir -p debian/usr/local/etc
	@cp pisound.conf debian/usr/local/etc/
	@fakeroot dpkg --build debian
	@mv debian.deb pisound-btn.deb
	@gunzip `find . | grep gz` > /dev/null 2>&1
