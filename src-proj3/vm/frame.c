#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "devices/timer.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/sup_page.h"

#define LATEST_CREATE_TIME 0xefffffff

void
initialize_frame_table(void)
{
  list_init(&frame_table);
  return;
}

bool
frame_init(struct frame* f, enum palloc_flags flag)
{
  bool success = false;

  /* Check the given ptr is valid or not */
  if(f == NULL){
    goto done;
  }

  /* Initialize frame elements */
  lock_init(&f->f_lock);
  f->allocator = thread_current();
  f->pte = NULL;
  f->created_time = timer_ticks();
  f->locked = false;

  /* Try to allocate a frame */
  uint8_t* frame_base = frame_allocation(flag);
  f->frame_base = frame_base;
  if(frame_base == NULL){                     /* No more page can be allocated from memory */
    /* Need to do eviction */
    struct frame* victim_frame = next_frame_to_evict();
    size_t swap_idx = write_into_swap_space(victim_frame->user_vaddr);
    
    success = try_to_evict(victim_frame, swap_idx);
    if(success){
      // alloc a new frame here.
      frame_base = frame_allocation(flag);
      f->frame_base = frame_base;
    }
    goto done;
  }

  success = true;
  
done:
  return success;
}

struct frame*
frame_create(enum palloc_flags flag)
{
  /* Allocate space */
  struct frame* f = (struct frame*) malloc(sizeof(struct frame));
  if(f == NULL){
    goto done;
  }

  bool is_init = frame_init(f, flag);

  /* push the new frame into the frame table */
  lock_acquire(&f->f_lock);
  list_push_back(&frame_table, &f->elem);
  lock_release(&f->f_lock);

  /* Initialize the frame */
  if(!is_init){
    free_frame(f);
    f = NULL;
    goto done;
  }

done:
  return f;
}

uint8_t*
frame_allocation(enum palloc_flags flag)
{
  uint8_t* frame_base = NULL;
  if(flag == PAL_USER | PAL_ZERO){
    frame_base = (uint8_t*)palloc_get_page(PAL_USER | PAL_ZERO);
  }
  else{
    if(flag == PAL_USER){
      frame_base = (uint8_t*)palloc_get_page(PAL_USER);
    }
  }
  return frame_base;
}

bool
free_frame(struct frame* f)
{
  bool success = false;
  
  /* Check the given ptr is valid or not */
  if(f == NULL){
    goto done;
  }

  /* Free the frame allocated before */
  if(f->frame_base != NULL){
    palloc_free_page (f->frame_base);
  }

  /* Remove and free this frame table entry */
  lock_acquire(&f->f_lock);
  list_remove(&f->elem);
  lock_release(&f->f_lock);
  free(f);

  success = true;

done:
  return success;
}

struct frame*
find_frame_table_entry_by_frame(uint8_t* f)
{
  struct frame* fe = NULL;
  for(struct list_elem* iter = list_begin(&frame_table);
                        iter != list_end(&frame_table);
                        iter = list_next(iter)){
    fe = list_entry(iter, struct frame, elem);
    if(fe->frame_base == f){
      return fe;
    }
  }
  return fe;
}

void
set_pte_to_given_frame(uint8_t* frame_base, uint32_t* pte, void* user_ptr)
{
  struct frame* fe = find_frame_table_entry_by_frame(frame_base);
  if(!fe){
    return;
  }
  fe->pte = pte;
  fe->user_vaddr = user_ptr;
  return;
}

/* Use LRC(Least Recently Created) mechanism to evict */
struct frame*
next_frame_to_evict(void)
{
  unsigned create_time = LATEST_CREATE_TIME;
  struct frame* target_fe = NULL;

  for(struct list_elem* iter = list_begin(&frame_table);
                        iter != list_end(&frame_table);
                        iter = list_next(iter)){
    struct frame* fe = list_entry(iter, struct frame, elem);
    if(fe->created_time < create_time && !fe->locked){
      target_fe = fe;
      create_time = fe->created_time;
    }
  }

  return target_fe;
}

bool
try_to_evict(struct frame* f, size_t swap_idx)
{
  printf("try_to_evict\n");
  ASSERT(f != NULL);
  ASSERT(f->frame_base != NULL && is_kernel_vaddr(f->frame_base));
  ASSERT(f->pte != NULL);
  ASSERT(f->user_vaddr != NULL && is_user_vaddr(f->user_vaddr));
  ASSERT(f->allocator != NULL && f->allocator->magic == 0xcd6abf4b);

  bool success = false;

  struct thread* allocator_t = f->allocator;

  ASSERT(pagedir_get_page(allocator_t->pagedir, f->user_vaddr));

  struct supp_page* spge = find_fake_pte(&allocator_t->page_table, pg_round_down(f->user_vaddr));
  if(spge == NULL){     /* No corresponding supplemental page table entry */
    if(!create_evicted_pte(allocator_t, swap_idx, f->user_vaddr)){
      goto done;
    }
  }
  else{
    if(!real2evicted_page_convert(spge, swap_idx)){
      goto done;
    }
  }

  pagedir_clear_page(allocator_t->pagedir, f->user_vaddr);
  free_frame(f);
  success = true;

done:
  return success;
}