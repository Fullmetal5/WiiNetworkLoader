#ifndef __FAT_H__
#define __FAT_H__

int fat_init(void);
int fat_open(const char *name);
int fat_read(void *data, u32 len);

#endif
