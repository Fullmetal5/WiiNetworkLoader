#ifndef __SD_H__
#define __SD_H__

int sd_init(void);
int sd_read_sector(u8 *data, u32 offset);
int sd_close(void);

#endif
