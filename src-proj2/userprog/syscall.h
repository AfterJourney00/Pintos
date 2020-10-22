#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
typedef int pid_t;

void syscall_init (void);

static void halt(void);
static void exit(int status);
static pid_t exec(const char* cmd_line);
static int wait(pid_t pid);
static int create(const char* file, unsigned initial_size);
static int remove(const char* file);
static int open(const char* file);
static int filesize(int fd);
static int read(int fd, void *buffer, unsigned size);
static int write(int fd, const void *buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);
#endif /* userprog/syscall.h */
