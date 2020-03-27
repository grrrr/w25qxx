/*
 * util.h
 *
 *  Created on: Mar 27, 2020
 *      Author: thomas
 */

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "w25qxx.h"

#define USE_DMA 1

#define FLASH_SECTORS (w25qxx.SectorCount)
#define PAGES_PER_SECTOR 16
#define SECTOR_SIZE (w25qxx.SectorSize)
#define PAGE_SIZE (w25qxx.PageSize)

typedef struct {
  int status;
  int run;
  uint32_t sector;
  int test_offs;
  uint8_t *todo_buffer;
  uint32_t todo_items;
  uint32_t tick, last;
  uint8_t *writebuf, *readbuf;
} flash_schedule_t;

void flash_schedule_init(flash_schedule_t *f);
int flash_schedule_next(flash_schedule_t *f);

#endif /* SRC_UTIL_H_ */
