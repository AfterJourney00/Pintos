#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

typedef int pid_t;

struct file_des
{
  int fd;
  int size;
  struct file *file_ptr;
  struct thread* opener;
  struct list_elem filelem;
};

void syscall_init (void);

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

/* Helper functions */
int bad_ptr(const char* file);
void clear_files(struct thread* t);

#endif /* userprog/syscall.h */
