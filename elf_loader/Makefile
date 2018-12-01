# Copyright 2018  Dexter Gerig  <dexgerig@gmail.com>
# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

CROSS ?= powerpc-eabi-


ifeq ($(origin AS), default)
	AS := $(CROSS)as
endif
ifeq ($(origin CC), default)
	CC := $(CROSS)gcc -m32
endif
ifeq ($(origin LD), default)
	LD := $(CROSS)ld
endif
OBJCOPY ?= $(CROSS)objcopy


CFLAGS := -g -Wall -W -O2 -ffreestanding -std=gnu99 -Wstrict-aliasing=2 \
	-mno-eabi -mno-sdata -mcpu=750

targets := loader.bin

objs := crt0.o main.o string.o ios.o elf.o sync.o time.o

ifeq ($(V),1)
	Q :=
else
	Q := @
	MAKEFLAGS += --no-print-directory
endif


all: $(targets)

$(targets): %.bin: %.elf
	@echo "  OBJCOPY   $@"
	$(Q)$(OBJCOPY) -O binary $< $@
	$(Q)xxd -i $@ elf_loader.h

elfs := $(targets:.bin=.elf)
$(elfs): %.elf: %.lds $(objs)
	@echo "  LINK      $@"
	$(Q)$(LD) $(LDFLAGS) -n -T $^ -o $@

%.o: %.c loader.h
	@echo "  COMPILE   $@"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

crt0.o: %.o: %.s
	@echo "  ASSEMBLE  $@"
	$(Q)$(AS) $< -o $@

FORCE:

clean:
	rm -rf $(objs) $(targets) $(elfs) elf_loader.h
