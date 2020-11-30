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
#include "vm/sup_page.h"

#define USER_STACK_BASE 0x08048000

typedef int pid_t;

static int global_fd = 1;       /* fd generator */
struct list file_list;          /* List for storing all opened files */
static uint32_t *stack_pointer; /* Functional stack pointer */

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);        /* Initialize file_lock */
  list_init(&file_list);        /* Initialize file list */
}

/* Bad pointer checker */
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

/* Find the corresponding file descriptor given a fd */
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

/* Clear all the files opened by thread t */
void
clear_files(struct thread* t)
{
  struct list_elem* iter = list_begin(&file_list);
  while(iter != list_end(&file_list)){
    struct list_elem* next_iter = list_next(iter);
    struct file_des* fdes = list_entry(iter, struct file_des, filelem);
    if(fdes->opener == t){
      list_remove(iter);
      file_close(fdes->file_ptr);
      free(fdes);
    }
    iter = next_iter;
  }
  return;
}

/* Function foe condition check of stack growth */
bool
is_request_extra_stack(void* ptr)
{
  return !(ptr < USER_STACK_BASE || (void*)stack_pointer - 32 > ptr);
}

/* Function for stack growing */
bool
grow_stack(void* ptr)
{
  bool success = false;
  struct frame* fe = frame_create(PAL_USER | PAL_ZERO);
  if(fe == NULL){
    goto done;
  }
  else{
    success = pagedir_set_page(thread_current()->pagedir, pg_round_down(ptr), fe->frame_base, true);
    if(!success){
      free_frame(fe);
    }
    else{
      /* Create a corresponding supplemental page table */
      success = supp_page_entry_create(CO_EXIST, NULL, 0, ptr, 0, 0, true);
    }
  }

done:
  return success;
}

/* Find mapping according to given id */
struct mmap_file_des*
find_map_by_id(mapid_t mapping)
{
  struct thread* cur = thread_current();
  struct mmap_file_des* target = NULL;

  for(struct list_elem* iter = list_begin(&cur->mmap_file_list);
                        iter != list_end(&cur->mmap_file_list);
                        iter = list_next(iter)){
    struct mmap_file_des* tmp = list_entry(iter, struct mmap_file_des, elem);
    if(tmp->id == mapping){
      target = tmp;
      break;
    }
  }

  return target;
}


/* Syscall handler */
static void
syscall_handler (struct intr_frame *f) 
{
  stack_pointer = f->esp;
  thread_current()->sp = stack_pointer;

  /* Check the interrupt stack valid or not */
  if(bad_ptr(f->esp) || bad_ptr((int*)(f->esp) + 1)
                     || bad_ptr((int*)(f->esp) + 2)
                     || bad_ptr((int*)(f->esp) + 3)){
    exit(-1);
  }
  
  /* Check the interrupt code is valid or not */
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
      char* cmd = (char*)*((int*)(f->esp) + 1);

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

    case SYS_SEEK:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);
      unsigned position = *((unsigned*)(f->esp) + 2);

      seek(fd, position);
      break;
    }

    case SYS_CLOSE:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);

      close(fd);
      break;
    }

    case SYS_MMAP:
    {
      /* parse the arguments first */
      int fd = *((int*)(f->esp) + 1);
      void* addr = (void*)*((int*)(f->esp) + 2);

      f->eax = mmap(fd, addr);
      break;
    }

    case SYS_MUNMAP:
    {
      /* parse the arguments first */
      int id = *((int*)(f->esp) + 1);

      munmap(id);
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

  /* Construct a exit_code_element */
  if(cur->parent_t != NULL){
    struct exit_code_list_element* exit_element = malloc(sizeof(struct exit_code_list_element));
    exit_element->thread_tid = cur->tid;
    exit_element->thread_exit_code = status;

    /* push this exit_code_element into children_exit_code_list of parent */
    list_push_back(&(cur->parent_t->children_exit_code_list), &(exit_element->elem));
  }
  
  /* Print termination information */
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit();
  return;
}

/* syscall: EXEC */
pid_t
exec(char* cmd_line)
{
  /* First, check the pointer is valid or not */
  if(bad_ptr(cmd_line) || bad_ptr(cmd_line + strlen(cmd_line))){
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

  /* Synchronization: only current thread access pointer file */
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

  if(file_opened != NULL){
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
  struct file_des* f = find_des_by_fd(fd);      /* Find the target file descriptor */
  if(f == NULL){                                /* If no target file descriptor */
    return -1;                                  /* return -1 */
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
  if(buffer == NULL || !is_user_vaddr(buffer)){
    exit(-1);
  }
  else{
    unsigned advance = PGSIZE;
    int size_copy = (int)size;
    
    for(void* ptr = buffer; ptr != NULL; ptr += advance){
      
      /* Check the validity of pointer */
      if(ptr == NULL || !is_user_vaddr(ptr)){
        exit(-1);
      }

      /* Check whether we need to load a fake page */
      if(!pagedir_get_page(thread_current()->pagedir, ptr)){
        /* Check the file user request is a fake_pte or not */
        struct supp_page* pte = find_fake_pte(&thread_current()->page_table, pg_round_down(ptr));
        if(pte == NULL){                    /* If no supplemental information stored */
          if(is_request_extra_stack(ptr)){
            grow_stack(ptr);                /* Need to grow stack */
          }
          else{
            exit(-1);
          }
        }
        else{                               /* If supplemental information exists */
          if(pte->fake_page){               /* If this is a fake page */
            fake2real_page_convert(pte);    /* Allocate space for this, lazy load */
          }
          else{                             /* Impossible real page but not found */
            exit(-1);
          }
        }
      }

      size_copy -= PGSIZE;
      if(size_copy <= 0){
        break;
      }
      else if(size_copy - PGSIZE <= 0){
        advance = PGSIZE - size_copy;
      }
      else{
        advance = PGSIZE;
      }
    }
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
  /* First check the buffer is a bad ptr or not */
  if(bad_ptr(buffer)){
    exit(-1);
  }

  int res;

  /* Synchronization: do write operation holding the file_lock */
  lock_acquire(&file_lock);
  
  if(fd == STDIN_FILENO){           /* WRITE syscall, do not support STDIN */
    res = -1;
  }
  else if(fd == STDOUT_FILENO){     /* If STDOUT mode */
    putbuf(buffer, size);
    res = size;
  }
  else{
    struct file_des *f = find_des_by_fd(fd);    /* Find the target file descriptor */
    if(f == NULL){                              /* If no target file descriptor */
      res = -1;                                 /* return -1 */
    }
    else{
      res = file_write(f->file_ptr, buffer, size);
    }
  }

  lock_release(&file_lock);
  return res;
}

/* syscall: SEEK */
void
seek(int fd, unsigned position)
{
  lock_acquire(&file_lock);

  struct file_des* f = find_des_by_fd(fd);    /* Find the target file descriptor */
  if(f == NULL){                              /* If no target file descriptor */
    goto done;      
  }
  else{
    if(f->file_ptr == NULL){                  /* If the file in file descriptor invalid*/
      goto done;
    }
    else{
      file_seek (f->file_ptr, position);
    }
  }

done:
  lock_release(&file_lock);
  return;
}

/* syscall: TELL */
unsigned tell(int fd)
{
  unsigned res;
  lock_acquire(&file_lock);

  struct file_des* f = find_des_by_fd(fd);    /* Find the target file descriptor */
  if(f == NULL){                              /* If no target file descriptor */
    res = 0;
    goto done;
  }
  else{
    if(f->file_ptr == NULL){                  /* If the file in file descriptor invalid*/
      res = 0;
      goto done;
    }
    else{
      res = file_tell(f->file_ptr);
    }
  }

done:
  lock_release(&file_lock);
  return res;
}

/* syscall: CLOSE */
void
close(int fd)
{
  lock_acquire(&file_lock);

  struct file_des *f = find_des_by_fd(fd);    /* Find the target file descriptor */

  /* Check the file is valid or not and check the closer is also the opener or not */
  if(f == NULL || f->opener != thread_current()){
    goto done;
  }
  list_remove(&(f->filelem));
  file_close(f->file_ptr);
  free(f);

done:
  lock_release(&file_lock);
  return;
}

/* syscall: MMAP */
mapid_t
mmap(int fd, void *addr)
{
  ASSERT(is_user_vaddr(addr));

  mapid_t id = -1;

  /* Check the given parameters' validity */
  if(fd <= 1 || addr == NULL || pg_ofs(addr) != 0){
    return -1;
  }

  lock_acquire(&file_lock);
  
  /* Find the size of corresponding file */
  struct file_des *des = find_des_by_fd(fd);
  if(des == NULL || des->file_ptr == NULL || des->size <= 0){
    goto done;
  }
  int length = des->size;

  struct file* file_copy = file_reopen(des->file_ptr);
  if(file_copy == NULL){
    goto done;
  }
  ASSERT(length == file_length(file_copy));

  /* Check whether overlapping mapped memory exists */
  for(int advance = 0; advance < length; advance += PGSIZE){
    if(find_fake_pte(&thread_current()->page_table, pg_round_down(addr + advance))){
      goto done;
    }
  }

  uint32_t zero_bytes;
  uint32_t read_bytes;
  if(length <= PGSIZE){
    read_bytes = length;
    zero_bytes = PGSIZE - read_bytes;
    if(!supp_page_entry_create(LAZY_LOAD, file_copy, 0, addr, read_bytes, zero_bytes, true)){
      goto done;
    }
  }
  else{
    zero_bytes = (uint32_t)length % (uint32_t)PGSIZE;
    read_bytes = (uint32_t)length - (uint32_t)zero_bytes;
   
    /* Lazy load */
    int i = 0;
    for(i = 0; i < read_bytes / PGSIZE; i++){
      if(!supp_page_entry_create(LAZY_LOAD, file_copy, i * PGSIZE, addr + i * PGSIZE,
                                  PGSIZE, 0, true)){
        goto done;
      }
    }
    if(!supp_page_entry_create(LAZY_LOAD, file_copy, i * PGSIZE, addr + i * PGSIZE,
                                  PGSIZE - zero_bytes, zero_bytes, true)){
      goto done;
    }
  }

  /* Generate a mapid */
  id = list_size(&thread_current()->mmap_file_list);
  struct mmap_file_des* mf_des = malloc(sizeof(struct mmap_file_des));
  mf_des->id = id;
  mf_des->file_ptr = file_copy;
  mf_des->mapped_addr = addr;
  mf_des->length = length;
  list_push_back(&thread_current()->mmap_file_list, &mf_des->elem);

done:
  lock_release(&file_lock);
  return id;
}

void
munmap (mapid_t mapping)
{
  ASSERT(mapping >= 0);     /* Assert that the given mapping id is a valid one */
  
  /* Find the corresponding mapping from this process */
  struct mmap_file_des* mf_des = find_map_by_id(mapping);
  if(mf_des == NULL){
    exit(-1);
  }
  
  /* Find all supplemental page table entries belonging to this mapping */
  struct file* unmapping_file = mf_des->file_ptr;
  int length = mf_des->length;
  void* start_ptr = mf_des->mapped_addr;

  ASSERT(pg_round_down(start_ptr) == start_ptr);

  lock_acquire(&file_lock);
  
  for(int advance = 0; advance < length; advance += PGSIZE){
    struct supp_page* spge = find_fake_pte(&thread_current()->page_table, start_ptr + advance);
    int write_length = PGSIZE;
    if(advance + PGSIZE > length){
      write_length = length - advance;
    }
    if(!try_to_unmap(spge, advance, write_length)){  /* Try to unmap this spge */
      exit(-1);
    }  
    free(spge);                       /* Free the supplemental page table entry*/
  }
  
  /* Close the file and clear the memory mapped file descriptor */
  file_close(mf_des->file_ptr);
  list_remove(&mf_des->elem);

  lock_release(&file_lock);
  
  free(mf_des);

  return;
}