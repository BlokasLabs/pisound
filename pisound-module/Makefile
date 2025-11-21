# pisound Linux kernel module.
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

KERNELRELEASE ?= `uname -r`
KERNEL_DIR ?= /lib/modules/$(KERNELRELEASE)/build
PWD := $(shell pwd)

ifeq ($(DEBUG), yes)
	ccflags-y += -DDEBUG
else
	ccflags-y += -DNDEBUG
endif

obj-m := snd_soc_pisound.o
snd_soc_pisound-y := pisound.o

dtbo-y += pisound.dtbo
DTC_FLAGS ?= -@

always-y := $(dtbo-y)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

install: all
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install
	@depmod -a  $(KERNELRELEASE)
	@cp -v pisound.dtbo /boot/overlays/

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
