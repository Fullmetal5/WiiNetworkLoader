// Copyright 2017-2018  Dexter Gerig <dexgerig@gmail.com>
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "loader.h"

static u8 *const code_buffer = (u8 *)0x90100000;

static void dsp_reset(void)
{
	write16(0x0c00500a, read16(0x0c00500a) & ~0x01f8);
	write16(0x0c00500a, read16(0x0c00500a) | 0x0010);
	write16(0x0c005036, 0);
}

void main(void* input_elf, u32 input_elf_size)
{
	dsp_reset();

	// Unlock EXI.
	write32(0x0d00643c, 0);

	reset_ios();
	
	memcpy(code_buffer, input_elf, input_elf_size);

	((void(*)())load_elf_image(code_buffer))();
}

void __eabi(){}
