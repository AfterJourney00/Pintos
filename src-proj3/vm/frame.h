#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"

/* Frame table, every entry is a frame */
struct list frame_table;
struct lock frame_lock;

/* Frame */
struct frame{
  uint8_t *frame_base;          /* Base address */
  struct thread* allocator;     /* The frame's allocator */
  uint32_t *pte;                /* Record the corresponding page table entry */
  int64_t created_time;         /* Record the time this frame is created */
  void* user_vaddr;             /* Record the corresponding user virtual address */
  bool locked;                  /* Indicate whether the frame can be evicted */
  struct list_elem elem;        /* Element for list */
};

/* Initialization of frame_table, used in init.c */
void initialize_frame_table(void);

/* Basic lifecycle of frame */
bool frame_init(struct frame* f, enum palloc_flags flag);
struct frame* frame_create(enum palloc_flags flag);
uint8_t* frame_allocation(enum palloc_flags flag);
bool free_frame(struct frame* f);

/* Functionality needed by other parts */
struct frame* find_frame_table_entry_by_frame(uint8_t* f);
void set_pte_to_given_frame(uint8_t* frame_base, uint32_t* pte, void* user_ptr);
struct frame* next_frame_to_evict(void);
bool try_to_evict(struct frame* f, size_t swap_idx);

#endif