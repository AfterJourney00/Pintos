#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

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

  /* Try to allocate a frame */
  uint8_t* frame_base = frame_allocation(flag);
  if(frame_base == NULL){
    goto done;
  }
  
  /* Initialize frame elements */
  f->frame_base = frame_base;
  lock_init(&f->f_lock);
  f->allocator = thread_current()->tid;
  f->pte = NULL;

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

  /* Initialize the frame */
  if(!frame_init(f, flag)){
    free_frame(f);
    f = NULL;
    goto done;
  }

  /* push the new frame into the frame table */
  lock_acquire(&f->f_lock);
  list_push_back(&frame_table, &f->elem);
  lock_release(&f->f_lock);

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
set_pte_to_given_frame(uint8_t* frame_base, uint8_t* pte)
{
  struct frame* fe = find_frame_table_entry_by_frame(frame_base);
  if(!fe){
    return;
  }
  fe->pte = pte;
  return;
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