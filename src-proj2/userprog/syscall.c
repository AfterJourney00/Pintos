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

typedef int pid_t;

struct file_des
{
  int fd;
  int size;
  struct file *file_ptr;
  struct thread* opener;
  struct list_elem filelem;
};

struct lock file_lock;
static int global_fd = 1;
struct list file_list;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
  list_init(&file_list);
}

/* Some helper functions */
int
bad_ptr(const char* file)
{
  /* check whether the ptr is NULL or is not in user virtual address */
  if(!(file && is_user_vaddr(file))){
    return 1;
  }
  /* check whether the ptr is unmapped */
  if(!pagedir_get_page(thread_current()->pagedir, file)){
    return 1;
  }
  return 0;
}

struct file_des*
find_des_by_fd(int fd)
{
  struct file_des* target_file = NULL;
  for(struct list_elem* iter = list_begin(&file_list);
                        iter != list_end(&file_list);
                        iter = list_next(iter)){
    struct file_des* f = list_entry(iter, struct file_des, filelem);
    if(f->fd == fd){
      target_file = f;
      break;
    }
  }

  return target_file;
}

void
clear_files(struct thread* t)
{
  ASSERT(t == thread_current());

  for(struct list_elem* iter = list_begin(&file_list);
                        iter != list_end(&file_list);
                        iter = list_next(iter)){
    struct file_des* fdes = list_entry(iter, struct file_des, filelem);
    printf("hereee\n");
    printf("fdes->opener: %p\n", fdes->opener);
    if(fdes->opener == t){
      printf("hereee11\n");
      list_remove(iter);
      printf("hereee12\n");
      file_close(fdes->file_ptr);
      printf("hereee13\n");
      free(fdes);
      printf("hereee14\n");
    }
    else{
      printf("hereee12w\n");
    }
  }

  return;
}


/* Syscall handler */
static void
syscall_handler (struct intr_frame *f) 
{
  /* if the interrupt stack is not in range of user address space */
  /* exit(-1) */
  if(bad_ptr(f->esp) || bad_ptr((int*)(f->esp) + 1)
                     || bad_ptr((int*)(f->esp + 2))
                     || bad_ptr((int*)(f->esp + 3))){
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
    case SYS_HALT:
    {
      halt();
      break;
    }

    case SYS_EXIT:
    {
      /* parse the arguments first */
      int status = *((int*)(f->esp) + 1);
      exit(status);
      break;
    }

    case SYS_EXEC:
    {
      /* parse the arguments first */
      const char* cmd = (const char*)*((int*)(f->esp) + 1);
      f->eax = exec(cmd);
      break;
    }

    case SYS_WAIT:
    {
      /* parse the arguments first */
      pid_t pid = *((pid_t*)(f->esp) + 1);
      f->eax = wait(pid);
      break;
    }

    case SYS_CREATE:
    {
      /* parse the arguments first */
      const char* file = (const char*)*((int*)(f->esp) + 1);
      unsigned size = (unsigned)*((int*)(f->esp) + 2);
      f->eax = create(file, size);
      break;
    }

    case SYS_REMOVE:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);
      f->eax = remove(fd);
      break;
    }

    case SYS_OPEN:
    {
      /* parse the arguments first */
      const char* file = (const char*)*((int*)(f->esp) + 1);
      f->eax = open(file);
      break;
    }

    case SYS_FILESIZE:
    {
      /* parse the arguments first */
      int file_id = *((int*)(f->esp) + 1);
      f->eax = filesize(file_id);
      break;
    }

    case SYS_READ:
    {
      /* parse the arguments first */
      int file_id = *((int*)(f->esp) + 1);
      void* buffer = (void*)*((int*)(f->esp) + 2);
      unsigned size = *((unsigned*)(f->esp) + 3);
      f->eax = read(file_id, buffer, size);
      break;
    }

    case SYS_WRITE:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);
      const void* buffer = (const void*)*((int*)(f->esp) + 2);
      unsigned size = *((unsigned*)(f->esp) + 3);
      f->eax = write(fd, buffer, size);
      break;
    }

    case SYS_CLOSE:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);
      close(fd);
      break;
    }
  }
}

/* syscall: HALT */
void 
halt(void)
{
  shutdown_power_off();
}

/* syscall: EXIT */
void
exit(int status)
{
  struct thread *cur = thread_current();
  cur->exit_code = status;

  intr_disable();
  printf ("%s: exit(%d)\n", cur->name, status);
  intr_enable();
  thread_exit();
  return;
}

/* syscall: EXEC */
pid_t
exec(const char* cmd_line)
{
  /* First, check the pointer is valid or not */
  if(bad_ptr(cmd_line)){
    exit(-1);
  }
  return process_execute(cmd_line);
}

/* syscall: WAIT */
int
wait(pid_t pid)
{
  return process_wait(pid);
}

/* syscall: CREATE */
int 
create(const char* file, unsigned initial_size)
{
  /* Check the pointer is valid or not */
  if(bad_ptr(file)){
    exit(-1);
  }

  /* Synchronization: only current thread access pointer file */
  lock_acquire(&file_lock);
  int success = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return success;
}

/* syscall: REMOVE */
int
remove(const char* file)
{
  /* Check the pointer is valid or not */
  if(bad_ptr(file)){
    exit(-1);
  }

  int success;
  lock_acquire(&file_lock);
  success = filesys_remove(file);
  lock_release(&file_lock);
  return success;
}

/* syscall: OPEN */
int
open(const char* file)
{
  /* Check the pointer is valid or not */
  if(bad_ptr(file)){
    exit(-1);
  }  

  /* Synchronization: only current thread access pointer file */
  lock_acquire(&file_lock);

  struct file *file_opened = filesys_open(file); 
  struct file_des* des;

  /* If open successfully */
  if(file_opened != NULL){
    thread_current()->file_running = file;
    des = (struct file_des*)malloc(sizeof(struct file_des));
    des->file_ptr = file_opened;
    des->fd = ++global_fd;                       /* Set the fd */
    des->size = file_length(file_opened);        /* Set the size of file */
    des->opener = thread_current();              /* Set the opener thread */
    list_push_back(&file_list, &(des->filelem)); /* Push this descriptor into list */

    lock_release(&file_lock);

    return global_fd;
  }
  else{
    lock_release(&file_lock);
    return -1;
  }
}

/* syscall: FILESIZE */
int
filesize(int fd)
{
  struct file_des* f = find_des_by_fd(fd);
  if(f == NULL){
    return -1;
  }
  else{
    return f->size;
  }
}

/* syscall: READ */
int
read(int fd, void *buffer, unsigned size)
{
  /* First check the buffer is a bad ptr or not */
  if(bad_ptr(buffer)){
    exit(-1);
  }
  
  struct file_des* f;
  int success = 0;

  /* Synchronization: do read operation holding the file_lock */
  lock_acquire(&file_lock);

  if(fd == STDOUT_FILENO){          /* READ syscall, do not support STDOUT */
    success = -1;
  }
  else if(fd == STDIN_FILENO){      /* If STDIN mode */
    void* ptr = buffer;
    while(!bad_ptr(ptr + 1) && (ptr - buffer) < size - 1){    /* check bad ptr or oversize */
      *(uint8_t*)ptr = input_getc();
      ptr ++;
    }
    *(uint8_t*)ptr = 0;                                       /* Fill the 0 at the end */
    success = (int)((uint32_t)ptr - (uint32_t)buffer + 1);
  }
  else{
    f = find_des_by_fd(fd);         /* Find the target file descriptor */
    if(f == NULL){                  /* If no target file descriptor */
      success =  -1;                /* return -1 */
    }
    else{
      success = file_read(f->file_ptr, buffer, size);
    }
  }
  lock_release(&file_lock);
  return success;
}

/* syscall: WRITE */
int
write(int fd, const void *buffer, unsigned size)
{
  if(bad_ptr(buffer)){
    exit(-1);
  }

  int res;

  /* Synchronization: do write operation holding the file_lock */
  lock_acquire(&file_lock);
  
  if(fd == STDIN_FILENO){
    res = -1;
  }
  else if(fd == STDOUT_FILENO){
    intr_disable(); 
    putbuf(buffer, size);
    intr_enable();
    res = size;
  }
  else{
    struct file_des *f = find_des_by_fd(fd);
    if(f == NULL){
      res = -1;
    }
    else{
      res = file_write(f->file_ptr, buffer, size);
    }
  }

  lock_release(&file_lock);
  return res;
}

void
close(int fd)
{
  struct file_des *f = find_des_by_fd(fd);
  if(f == NULL || f->opener != thread_current()){
    return;
  }
  else{
    lock_acquire(&file_lock);

    list_remove(&(f->filelem));
    free(f);
    /*for(struct list_elem* iter = list_begin(&file_list);
                          iter != list_end(&file_list);
                          iter = list_next(iter)){
      struct file_des * temp = list_entry(iter, struct file_des, filelem);
      if(temp->fd = fd){
        list_remove(iter);
        file_close(temp->file_ptr);
        free(f);
        break;
      }
    }*/
    lock_release(&file_lock);
    return;
  }
}