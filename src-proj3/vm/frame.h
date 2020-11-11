#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"

/* Frame table, every entry is a frame */
struct list frame_table;

/* Frame */
struct frame{
  uint8_t *frame_base;          /* Base address */
  struct lock f_lock;           /* Lock per frame */
  tid_t allocator;              /* The frame's allocator */
  struct list_elem elem;        /* Element for list */
};

/* Operations on frame_table */
void initialize_frame_table(void);

/* Operations on frame */
bool frame_init(struct frame* f, enum palloc_flags flag);
struct frame* frame_create(enum palloc_flags flag);
uint8_t* frame_allocation(enum palloc_flags flag);
bool free_frame(struct frame* f);
void evict(struct frame* f);




#endif