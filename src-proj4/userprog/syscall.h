#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/sup_page.h"

typedef int pid_t;

struct lock file_lock;          /* Lock for file operations */

/* File descriptor */
struct file_des
{
  int fd;                             /* File descriptor number */
  int size;                           /* Size of this file */
  struct file *file_ptr;              /* The pointer of this file */
  struct thread* opener;              /* The thread open this file */
  struct list_elem filelem;           /* Element for list */
};

void syscall_init (void);

/* System calls */
void halt(void);
void exit(int status);
pid_t exec(char* cmd_line);
int wait(pid_t pid);
int create(const char* file, unsigned initial_size);
int remove(const char* file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

mapid_t mmap(int fd, void *addr);
void munmap (mapid_t mapping);

/* Helper functions */
int bad_ptr(const char* file);
void clear_files(struct thread* t);
bool is_request_extra_stack(void* ptr);
bool grow_stack(void* ptr);
struct mmap_file_des* find_map_by_id(mapid_t mapping);

#endif /* userprog/syscall.h */
