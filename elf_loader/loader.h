// Copyright 2017-2018  Dexter Gerig  <dexgerig@gmail.com>
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#ifndef _LOADER_H
#define _LOADER_H

#include <stddef.h>


// String functions.

size_t strlen(const char *);
void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);


// Basic types.

typedef signed char s8;
typedef unsigned char u8;
typedef signed short int s16;
typedef unsigned short int u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long int s64;
typedef unsigned long long int u64;


// Basic I/O.

static inline u32 read32(u32 addr)
{
	u32 x;

	asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write32(u32 addr, u32 x)
{
	asm("stw %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}

static inline u16 read16(u32 addr)
{
	u16 x;

	asm volatile("lhz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write16(u32 addr, u16 x)
{
	asm("sth %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}


// Address mapping.

static inline u32 virt_to_phys(const void *p)
{
	return (u32)p & 0x7fffffff;
}


// Cache synchronisation.

void sync_before_read(void *p, u32 len);
void sync_after_write(const void *p, u32 len);
void sync_before_exec(const void *p, u32 len);


// Time.

void udelay(u32 us);


// Exceptions.

void exception_init(void);


// ELF.

void *load_elf_image(void *addr);


// IOS.

struct ioctlv {
	void *data;
	u32 len;
};

int ios_open(const char *filename, u32 mode);
int ios_close(int fd);
int ios_ioctl(int fd, u32 n, const void *in, u32 inlen, void *out, u32 outlen);

void reset_ios(void);


#endif
