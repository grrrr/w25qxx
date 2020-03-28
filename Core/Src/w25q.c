/*
 * w25q.c
 *
 *  Created on: Mar 28, 2020
 *      Author: thomas
 */

#include "w25q.h"


enum {
  STATUS_INIT,
  STATUS_LOOP_START,
  STATUS_READ_START,
  STATUS_READ_NEXT,
  STATUS_READ_END,
  STATUS_ERASE_BLOCK,
  STATUS_ERASE_CHECK,
  STATUS_WRITE_START,
  STATUS_WRITE_NEXT,
  STATUS_WRITE_END,
  STATUS_LOOP_END,
};

void flash_schedule_init(flash_schedule_t *f)
{
  f->status = STATUS_INIT;
}

int flash_schedule_next(flash_schedule_t *f)
{
  int waiting = 0;

  switch(f->status) {

    case STATUS_INIT: {
      f->loop = 0;
      f->sector = 0;

      f->status = STATUS_LOOP_START;
      break;
    }

    case STATUS_LOOP_START: {
      f->status = STATUS_READ_START;
      break;
    }

    case STATUS_READ_START: {
      f->status = STATUS_READ_NEXT;
      break;
    }

    case STATUS_READ_NEXT: {
      int rdy = 1; // TODO
      if(!rdy)
	waiting = 1;
      else
	f->status = STATUS_READ_END;
      break;
    }

    case STATUS_READ_END: {
      f->status = STATUS_ERASE_BLOCK;
      break;
    }

    case STATUS_ERASE_BLOCK: {
      if(f->sector%(32/4) == 0)
	W25qxx_Erase32k(f->sector/(32/4));
      f->status = STATUS_ERASE_CHECK;
      break;
    }

    case STATUS_ERASE_CHECK: {
      f->status = STATUS_WRITE_START;
      break;
    }

    case STATUS_WRITE_START: {
      f->status = STATUS_WRITE_NEXT;
      break;
    }

    case STATUS_WRITE_NEXT: {
      int rdy = 1; // TODO
      if(!rdy)
	waiting = 1;
      else
	f->status = STATUS_WRITE_END;
      break;
    }

    case STATUS_WRITE_END: {
      f->status = STATUS_LOOP_END;
      break;
    }

    case STATUS_LOOP_END: {
      if(++f->sector >= w25qxx.SectorCount) {
	  ++f->loop;
	  f->sector = 0;
      }

      f->status = STATUS_LOOP_START;
      break;
    }
  }

  return waiting;
}

