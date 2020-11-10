#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

bool
frame_init(struct frame* f)
{
  bool success = false;

  /* Check the given ptr is valid or not */
  if(f == NULL){
    goto done;
  }

  uint8_t* frame_base = frame_allocation();
  if(frame_base == NULL){
    goto done;
  }

  /* Initialize frame elements */
  f->frame_base = frame_base;
  lock_init(&f->f_lock);
  f->allocator = thread_current()->tid;

  success = true;
  
done:
  return success;
}

struct frame*
frame_create(void)
{
  /* Allocate space */
  struct frame* f = (struct frame*) malloc(sizeof(struct frame));
  if(f == NULL){
    goto done;
  }

  /* Initialize the frame */
  if(!frame_init(f)){
    /*free_frame*/
    f = NULL;
    goto done;
  }

  /* push the new frame into the frame table */
  list_push_back(&frame_table, &f->elem);

done:
  return f;
}

uint8_t*
frame_allocation(enum palloc_flags flag)
{
  uint8_t* frame_base = NULL;
  if(flag == PAL_USER | PAL_ZERO){
    uint8_t* frame_base = (uint8_t*)palloc_get_page(PAL_USER | PAL_ZERO);
  }
  else{
    if(flag == PAL_USER){
      uint8_t* frame_base = (uint8_t*)palloc_get_page(PAL_USER);
    }
  }
  return frame_base;
}

bool
free_frame(struct frame* f)
{

}