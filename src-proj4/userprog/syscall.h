#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "filesys/directory.h"

typedef int pid_t;

/* File descriptor */
struct file_des
{
  int fd;                             /* File descriptor number */
  int size;                           /* Size of this file */
  int is_dir;                         /* Record this fd is for a ordinary file or a directory */
  struct dir* dir;                    /* Pointer of the directory */
  struct file *file_ptr;              /* The pointer of this file */
  struct thread* opener;              /* The thread open this file */
  struct list_elem filelem;           /* Element for list */
};

struct lock file_lock;                /* Lock for file operations */

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
int chdir(const char *dir);
int mkdir(const char *dir);
int readdir(int fd, char *name);
int isdir(int fd);
int inumber(int fd);

/* Helper functions */
int bad_ptr(const char* file);
void clear_files(struct thread* t);

#endif /* userprog/syscall.h */
