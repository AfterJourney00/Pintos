#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"

/* Frame table, every entry is a frame */
list frame_table;

/* Frame */
struct frame{
  uint8_t *frame_base;          /* Base address */
  struct lock f_lock;           /* Lock per frame */
  tid_t allocator;              /* The frame's allocator */
  struct list_elem elem;        /* Element for list */
};

bool frame_init(void);
struct frame* frame_create(void);
uint8_t* frame_allocation(enum palloc_flags flag);
bool free_frame(struct frame* f);
void evict(struct frame* f);




#endif