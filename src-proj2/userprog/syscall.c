#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/process.h"
#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  /* if the interrupt stack is not in range of user address space */
  /* exit(-1) */
  if(!is_user_vaddr(f->esp)){
    exit(-1);
  }

  /* if the interrupt code is incalid */
  /* exit(-1) */
  int intr_code = *(int*)(f->esp);
  if(intr_code < SYS_HALT || intr_code > SYS_INUMBER){
    exit(-1);
  }

  /* choose the corret syscall handler to handle interrupts */
  switch(intr_code){
    case SYS_EXIT:
    {
      /* parse the arguments first */
      int status = *((int*)(f->esp) + 1);
      exit(status);
    }

    case SYS_WRITE:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);
      const void* buffer = (const void*)*((int*)(f->esp) + 2);
      unsigned size = *((unsigned*)(f->esp) + 3);
      f->eax = write(fd, buffer, size);
    }

  }
}

static void
exit(int status)
{
  thread_current()->exit_code = status;
  thread_exit();
  return;
}

static int
write(int fd, const void *buffer, unsigned size)
{
  if(fd == 1){    
    putbuf(buffer, size);
    return size;
  }
}