 #############################################################################
 # Makefile - Control Area Network - Ethernet - Gateway (Kernel Module)
 #############################################################################

 #############################################################################
 # (C) Copyright 2013 Fabian Raab, Stefan Smarzly
 #
 # This file is part of CAN-Eth-GW.
 #
 # CAN-Eth-GW is free software: you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation, either version 3 of the License, or
 # (at your option) any later version.
 #6
 # CAN-Eth-GW is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 # GNU General Public License for more details.
 #
 # You should have received a copy of the GNU General Public License
 # along with CAN-Eth-GW.  If not, see <http://www.gnu.org/licenses/>.
 #############################################################################

# TODO remove Debugging mode
ccflags-y := -DDEBUG -std=gnu99 -Wno-declaration-after-statement
ccflags-y += -Wno-unused-label -Wno-unused-function
ccflags-y += -I$(PWD)/src -I$(PWD)/include

SRC := src/ce_gw_main.o
SRC += src/ce_gw_dev.o
SRC += src/ce_gw_netlink.o
OUTPUT := out

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-${CONFIG_CE_GW} += ${OUTPUT}/ce_gw.o
	${OUTPUT}/ce_gw-objs = $(SRC:.c=.o)

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	#Keep export CONFIG* until the sources moved into the kernel source tree
	export CONFIG_CE_GW=m

	KERNELDIR := /lib/modules/$(shell uname -r)/build
	# Path to target Linux Kernel
	PWD  := $(shell pwd)

modules modules_install clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $@

endif
