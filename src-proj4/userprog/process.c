#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (char **file_name, void (**eip) (void), void **esp, int argc);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{ 
  char *fn_copy;
  char* fn_for_parse;
  tid_t tid;
  char *exec_name = NULL, *save_ptr = NULL;    /* For parsing. */
  struct thread* cur = thread_current ();
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  
  /* Make another copy for parsing since FILE_NAME is a const one */
  fn_for_parse = palloc_get_page (0);
  if (fn_for_parse == NULL){
    palloc_free_page (fn_copy);     /* Free the space allocated before */
    return TID_ERROR;
  }
  strlcpy (fn_for_parse, file_name, PGSIZE);

  /* Parse file_name to have exec_name. */
  /* For example: 'args-single onearg' ===> 'args-single' & 'onearg' */
  exec_name = strtok_r(fn_for_parse, " ", &save_ptr);

  /* If the input is not a valid file_name */
  if (exec_name == NULL){
    /* Free the space allocated before */
    palloc_free_page (fn_copy);
    palloc_free_page(fn_for_parse);

    return TID_ERROR;
  }

  /* Create a new thread named FILE_NAME, run function start_process with paras save_ptr */
  tid = thread_create (exec_name, PRI_DEFAULT, start_process, fn_copy);
  
  /* Free useless space in time */
  palloc_free_page(fn_for_parse);
  
  /* Check thread create successfully or not */
  if(tid == TID_ERROR){
    palloc_free_page (fn_copy);
    return tid;
  }

  /* Find the pointer pointing to the child thread */
  struct thread* t = find_thread_by_tid(tid);
  if(t == NULL){
    palloc_free_page (fn_copy);
    return TID_ERROR;
  }

  /* Synchronization: use condition variable to wait child thread */
  lock_acquire(&(cur->loading_lock));
  while(!t->isloaded){
    cond_wait(&(cur->loading_cond), &(cur->loading_lock));
  }
  lock_release(&(cur->loading_lock));
  
  /* Check if the child thread has exited already, if yes, retrieve it */
  for(struct list_elem* iter = list_begin(&cur->children_exit_code_list);
                        iter != list_end(&cur->children_exit_code_list);
                        iter = list_next(iter)){
    struct exit_code_list_element* ece = list_entry(iter, struct exit_code_list_element, elem);
    if(ece->thread_tid == tid){
      return tid;
    }
  }
  
  /* The chile thread is not exited yet, so check load successfully or not */
  if(t->isloaded != 1){
    return -1;
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread* cur = thread_current();

  char *save_ptr;
  char **argv = (char**)malloc(128); /* Limit the arguments less than 128 bytes */
  int argc = 0;                      /* Count of arguments passed in on one command line. */

  /* argv[0] is the real file name, and remaining are arguments */
  /* argc = 1(real file name) + # of arguments */
  for (char *token = strtok_r ((char *)file_name, " ", &save_ptr);
       token != NULL;
       token = strtok_r (NULL, " ", &save_ptr))
  {
    argv[argc] = token;
    argc++; 
  }
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (argv, &if_.eip, &if_.esp, argc);
  
  /* free the memory allocated by malloc before */
  free(argv);

  if(success){              
    cur->isloaded = 1;     /* load success: set isloaded to 1 */
  }
  else{
    cur->isloaded = -1;    /* load fail: set isloaded to -1 */ 
  }

  if(cur->parent_t != NULL){
    if(cur->parent_t->cwd != NULL){
      cur->cwd = dir_reopen(cur->parent_t->cwd);
    }
    else{
      cur->cwd = dir_open_root();
    }

    /* Synchronization: loading finished, broadcast the condition variable */
    lock_acquire(&(cur->parent_t->loading_lock));
    cond_broadcast(&(cur->parent_t->loading_cond), &(cur->parent_t->loading_lock));
    lock_release(&(cur->parent_t->loading_lock));
  }

  if (!success){
    cur->waited = -1;      /* load fail: set waited to -1 */
    cur->exit_code = -1;   /* load fail: set exit_code to -1 */
    thread_exit ();
  }

  palloc_free_page (pg_round_down(file_name));

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread* cur = thread_current();

  /* Check the child_tid is valid or not */
  if(child_tid == TID_ERROR){
    return -1;
  }
  else{
    /* Check the child thread is exited or not, if yes, return its exit code directly */
    for(struct list_elem* iter = list_begin(&cur->children_exit_code_list);
                          iter != list_end(&cur->children_exit_code_list);
                          iter = list_next(iter)){
      struct exit_code_list_element* ece = list_entry(iter, struct exit_code_list_element, elem);
      if(ece->thread_tid == child_tid){
        list_remove(iter);
        int return_code = ece->thread_exit_code;      
        free(ece);
        return return_code;
      }
    }

    /* If child thread is not exited yet, try to wait it */
    struct thread* target_thread = NULL;
    for(struct list_elem* iter = list_begin(&cur->children_t_list);
                          iter != list_end(&cur->children_t_list);
                          iter = list_next(iter)){
      struct thread* t = list_entry(iter, struct thread, childelem);
      if(t->tid == child_tid){
        target_thread = t;
        break;
      }
    }
    
    /* If not found, current thread is not the parent of the child */
    if(target_thread == NULL){
      return -1;
    }
    else{
      /* Check the child thread is waited already or not */
      if(target_thread->waited != 0 || target_thread->status == THREAD_DYING){
        return -1;
      }
      else{
        target_thread->waited = 1;
        
        lock_acquire(&(cur->loading_lock));
        while(find_thread_by_tid(child_tid) != NULL){
          cond_wait(&(cur->loading_cond), &(cur->loading_lock));
        }
        lock_release(&(cur->loading_lock));
        

        /* Check the child thread is exited or not, if yes, return its exit code directly */
        struct exit_code_list_element* return_exit_element = NULL;
        for(struct list_elem* iter = list_begin(&(cur->children_exit_code_list));
                              iter != list_end(&(cur->children_exit_code_list));
                              iter = list_next(iter)){
          struct exit_code_list_element* ece = list_entry(iter,
                                                          struct exit_code_list_element, elem);
          if(ece->thread_tid == child_tid){
            return_exit_element = ece;
            list_remove(iter);
            break;
          }
        }
        int exit_status = -1;
        if(return_exit_element != NULL){
          exit_status = return_exit_element->thread_exit_code;
          free(return_exit_element);
        }
        return exit_status;
      }
    }
  }
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Clear thread's children list */
  for(struct list_elem* iter = list_begin(&(cur->children_t_list));
                        iter != list_end(&(cur->children_t_list));
                        iter = list_next(iter)){
    struct thread * t = list_entry(iter, struct thread, childelem);
    list_remove(iter);
  }  

  /* Clear children_exit_code_list */
  for(struct list_elem* iter = list_begin(&(cur->children_exit_code_list));
                        iter != list_end(&(cur->children_exit_code_list));
                        iter = list_next(iter)){
    struct exit_code_list_element * ece = list_entry(iter,
                                                     struct exit_code_list_element,
                                                     elem);
    list_remove(iter);
    free(ece);
  }

  /* Close those files opened by this thread */
  if (cur->file_running != NULL){
    file_allow_write(cur->file_running);
    cur->file_running = NULL;
  }

  if(cur->parent_t != NULL){
    list_remove(&cur->childelem);       /* remove current thread from its parent's children list */
    lock_acquire(&(cur->parent_t->loading_lock));
    list_remove (&cur->allelem);        /* remove current thread from all_list */
    cond_broadcast(&(cur->parent_t->loading_cond), &(cur->parent_t->loading_lock));
    lock_release(&(cur->parent_t->loading_lock));
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  
  /* Clear all files opened by current thread */
  clear_files(cur);

  /* Close the current working directory of the process */
  dir_close(cur->cwd);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char **argv, int argc);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (char **file_name, void (**eip) (void), void **esp, int argc) 
{
  /* Here, file_name[0] is the real file name, and remaining are arguments */
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();
  
  /* Open executable file. */
  file = filesys_open (file_name[0]);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name[0]);
      file_close (file);
      goto done; 
    }
  
  /* Set the file to current thread's running file */
  t->file_running = file;

  /* Deny writes to executable */
  file_deny_write (file);
  
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }
  
  /* Set up stack. */
  if (!setup_stack (esp, file_name, argc))
    goto done;
  
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char **argv, int argc) 
{
  /* esp is the pointer pointing to stack pointer, *esp is the stack pointer */
  uint8_t *kpage;
  bool success = false;
  
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        /* Offset of the PHYS_BASE. */
        /* *esp = PHYS_BASE - 12; */
        /* Initial the stack pointer as the base physical address */
        *esp = PHYS_BASE;

        /* A list of address in the stack. */
        char *argv_ptr[argc];
        
        for (int i = argc - 1; i >= 0; i--)
        {
          /* Allocate space for the command line. */
          *esp = *esp - (strlen (argv[i]) + 1);
          memcpy (*esp, argv[i], (strlen (argv[i]) + 1));
          argv_ptr[i] = *(char**)esp;
        }

        /* Word align. */
        int remainder = 4 - (*(uint8_t*)esp % 4);
        for(int i = 0; i < remainder; i++){
          *esp = *esp - 1;
          *(uint8_t *)(*esp) = (uint8_t)0;
        }

        /* Push the last null to the stack. */
        *esp = *esp - 4;
        *(char **)(*esp) = (char*)0;

        /* Push the addresses of args to the stack. */
        for (int i = argc - 1; i >= 0; i--)
        {
          *esp = *esp - 4;
          *(char**)(*esp) = argv_ptr[i];
        }

        /* Push the head of argv list into the stack */
        *esp = *esp - 4;
        *(char**)(*esp) = (char*)(*esp) + 4;

        /* Push the argc to the stack. */
        *esp = *esp - 4;
        *(int *)(*esp) = argc;

        /* Return address is here. */
        *esp = *esp - 4;
        *(void**)(*esp) = (void*)0;
      }
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}