# CS130 Project 1: Threads

# Design Document

*Group 31*

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Chengfeng Zhao zhaochf@shanghaitech.edu.cn


-----

## Part1: Argument Passing

### Data Structures

#### A1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

- 

### Algorithms

#### A2: Briefly describe how you implemented argument parsing.  How do you arrange for the elements of `argv[]` to be in the right order? How do you avoid overflowing the stack page?

***Answer:***

- Briefly describe how you implemented argument parsing.
  Firstly, in `process_execute()`, we added a step of `strtok_r()` to parse the `file_name` and get the first word of the command line, which is the stuff we need to run. Next, in `start_process()`, we split the `file_name` to get different arguments in the command line. We passed the arguments in the form of `char *argv[]` and also the count of arguments to the `load()` function for further operation with the stack. In `load()`, the `argv[]` and `argc` are passed to `setup_stack()` to setup the stack. In `setup_stack()`, we pushed the arguments, zeros to help with word-align, addresses of the arguments, count of arguments and the return address to the stack in order to setup the stack.

- How do you arrange for the elements of `argv[]` to be in the right order?
  Actually, during our implementation we did not worried a lot about this problem, as in the process of parsing the order is handled by the original command line. When we are setting up the stack, the biggest concern is to change the order when pushing arguments to the stack. 
- How do you avoid overflowing the stack page?
  We handed it over to the page fault exception. This method is more efficient than any other ways considering to check the overflowing problem in the process  of setting up the stack.

### Rationale

#### A3: Why does Pintos implement `strtok_r()` but not `strtok()`?

***Answer:***

The Pintos kernel separates command line into executable name and arguments. So we need to make the address of arguments reachable after calling the splitting function. The `strtok_r()` function asks for a placeholder from the caller, which makes it more stable when it comes to splitting the command line.

#### A4: In Pintos, the kernel separates commands into a executable name and arguments. In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.

***Answer:***

1. Shorter time in kernel, more efficient.
2. Checking commands before kernel to prevent kernel from failing.
3. Shell would be able to pre-process the command, like a interpreter to make it more efficient. 

## Part2: System Calls

### Data Structures

#### B1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

These are for struct `thread`:

```C
struct thread* parent_t;             /* Record this thread's parent */
struct list children_t_list;         /* List of children threads */
struct list_elem childelem;          /* List element for children threads list. */    
struct file* file_running;           /* The file run by this thread */
struct lock loading_lock;            /* lock for parent thread to wait */
struct condition loading_cond;       /* cond variable for parent thread to wait */
int isloaded;                        /* The file loaded to the thread or not */
int waited;                          /* Record the thread waited, not waited, or terminated */
bool exited;                         /* Record whether the thread is exited */
int exit_code;                       /* The exit code returned when the thread exits */
struct list children_exit_code_list; /* A list used to record children threads' exit code*/
```

These are for a new struct `file_des` in `syscall.h`, which is a detailed version for file descriptors:

```C
struct file_des
{
  int fd;
  int size;
  struct file *file_ptr;
  struct thread* opener;
  struct list_elem filelem;
}
```

#### B2: Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

### Algorithms

#### B3: Describe your code for reading and writing user data from the kernel.

#### B4: Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel.  What is the least and the greatest possible number of inspections of the page table (e.g. calls to `pagedir_get_page()`) that might result?  What about for a system call that only copies 2 bytes of data?  Is there room for improvement in these numbers, and how much?

#### B5: Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

#### B6: Any access to user program memory at a user-specified address can fail due to a bad pointer value.  Such accesses must cause the process to be terminated.  System calls are fraught with such accesses, e.g. a "write" system call requires reading the system call number from the user stack, then each of the call's three arguments, then an arbitrary amount of user memory, and any of these can fail at any point.  This poses a design and error-handling problem: how do you best avoid obscuring the primary function of code in a morass of error-handling?  Furthermore, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed?  In a few paragraphs, describe the strategy or strategies you adopted for managing these issues.  Give an example.

### Synchronization

#### B7: The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading.  How does your code ensure this?  How is the load success/failure status passed back to the thread that calls "exec"?

#### B8: Consider parent process P with child process C.  How do you ensure proper synchronization and avoid race conditions when P calls wait(C) before C exits?  After C exits?  How do you ensure that all resources are freed in each case?  How about when P terminates without waiting, before C exits?  After C exits?  Are there any special cases?

### Rationale

#### B9: Why did you choose to implement access to user memory from the kernel in the way that you did?

#### B10: What advantages or disadvantages can you see to your design for file descriptors?

#### B11: The default `tid_t` to `pid_t` mapping is the identity mapping. If you changed it, what advantages are there to your approach?

## Survey Questions

Answering these questions is optional, but it will help us improve the course in future quarters.  Feel free to tell us anything you want--these questions are just to spur your thoughts.  You may also choose to respond anonymously in the course evaluations at the end of the quarter.

In your opinion, was this assignment, or any one of the three problems in it, too easy or too hard?  Did it take too long or too little time?

Did you find that working on a particular part of the assignment gave you greater insight into some aspect of OS design?

Is there some particular fact or hint we should give students in future quarters to help them solve the problems?  Conversely, did you find any of our guidance to be misleading?

Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects?

Any other comments?