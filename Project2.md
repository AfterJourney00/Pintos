# CS130 Project 2: User Programs

# Design Document

*Group 31*

- Yuqing Yao yaoyq@shanghaitech.edu.cn
  - Argument Passing
- Chengfeng Zhao zhaochf@shanghaitech.edu.cn
  - System Calls


-----



## Part1: Argument Passing

### ----- Data Structures -----

#### >>A1: Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or `enumeration`.  Identify the purpose of each in 25 words or less.

***Answer:***

No new structure is introduced in this part.



### ----- Algorithms -----

#### >>A2: Briefly describe how you implemented argument parsing.  How do you arrange for the elements of `argv[]` to be in the right order? How do you avoid overflowing the stack page?

***Answer:***

- ***Briefly describe how you implemented argument parsing.***

  Firstly, in `process_execute()`, we use `strtok_r()` to parse a copy of `file_name` and to get the first token of the command line, which is the real stuff we need to execute and pass into `thread_create()`. 

  Next, in `start_process()`, we allocate an array of `char*` named `argv` to record each argument token. Then we use `strtok_r()` in a loop to split the command line into tokens from head to tail and store them in `argv[argc]`, where `argc` is a counter to count the amount of arguments in this command line simultaneously. 

  The final step is stack setup. We pass `argv` and `argc` obtained before to the `load()` and in `load()`, the `argv` and `argc` are are passed to `setup_stack()`. Therefore, in `setup_stack()`, we can push arguments into stack in order. More than that, we also push zeros to help with word-align, addresses of the arguments, count of arguments and the return address to the stack just as what is described in official document.

- ***How do you arrange for the elements of `argv[]` to be in the right order?***
  Actually, during our implementation we did not worried a lot about this problem, as in the process of parsing the order is handled by the original command line. We just parse the whole command line from head token to tail token and store them in an array in order, and then push them onto stack also in order, no extra arrangement is needed in our implementation.
  
- ***How do you avoid overflowing the stack page?***
  We handed it over to the page fault exception. This method is more efficient than any other ways considering to check the overflowing problem in the process  of setting up the stack.



### ----- Rationale -----

#### >>A3: Why does Pintos implement `strtok_r()` but not `strtok()`?

***Answer:***

The Pintos kernel separates command line into executable name and arguments. So we need to make the address of arguments reachable after calling the splitting function. The `strtok_r()` function asks for a placeholder from the caller, which makes it more stable when it comes to splitting the command line.



#### >>A4: In Pintos, the kernel separates commands into a executable name and arguments. In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.

***Answer:***

1. Shorter time in kernel, more efficient.
2. Checking commands before kernel to prevent kernel from failing.
3. Shell would be able to pre-process the command, like a interpreter to make it more efficient. 



## Part2: System Calls

### ----- Data Structures -----

#### >>B1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

New structure in *thread.h*:

```c
/* Record a thread's exit status */
struct exit_code_list_element{
   tid_t thread_tid;                 /* The id of this thread */
   int thread_exit_code;             /* The exit_code of this thread */
   struct list_elem elem;            /* Element for list */
};
```

This structure is used to retrieve an exited and freed thread's information. We need it since sometimes child thread can exit before function `wait()` is called or returned.

------

Added to struct `thread`:

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

------

New structure in *syscall.h*:

```C
/* File descriptor */
struct file_des
{
  int fd;                             /* File descriptor number */
  int size;                           /* Size of this file */
  struct file *file_ptr;              /* The pointer of this file */
  struct thread* opener;              /* The thread open this file */
  struct list_elem filelem;           /* Element for list */
};
```

------

Added to *syscall.c*:

```c
typedef int pid_t;

struct lock file_lock;                /* A lock for making file operation mutually exclusive */
static int global_fd = 1;             /* A global counter for allocate new fd number */
struct list file_list;                /* A list to link all opened files */
```



#### >>B2: Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

***Answer:***

- ***Describe how file descriptors are associated with open files***:

  In our implementation, the association between file descriptors and open files is a full association. That is to say, as long as the syscall `open()` is called and the file user wants to open is a valid one, a corresponding file descriptor will be allocated, with the unique id `fd`.

- ***Are file descriptors unique within the entire OS or just within a single process?***

  File descriptors are unique within the entire OS in our implementation. Every process can use syscall to enter kernel and look up all currently opened files. Since we need to save space in PCB as more as possible, implement file descriptors unique within a single process may have potential threats if some processes open a lot of files. Also, we can specify each file descriptors for processes even implement them unique within the entire OS actually, so it's reasonable.



### ----- Algorithms -----

#### >>B3: Describe your code for reading and writing user data from the kernel.

***Answer:***

- ***Read:*** `int read(int fd, void *buffer, unsigned size)`

  1. Check the argument `buffer` is a valid pointer or not. If not, `exit(-1)` directly.

  2. Acquire `file_lock` to ensure the safety of critical section.

  3. Branch according specified `fd`:

     -  `if(fd == STDOUT_FILENO)`, set return value to `-1`.

     - `else if(fd == STDIN_FILENO)`

       -- read in input from keyboard till the address of next byte is invalid *or* `size` bytes of data has been read.

       -- fill in the terminator `'\0'` at the end.

       -- set return value to the number of successfully read bytes.

     - `else`

       -- use function `find_des_by_fd()` to find the file descriptor whose `fd` is equal to the argument.

       -- check the descriptor we found is valid or not, and set the return value to `-1` if not.

       -- use `file_read()` to read the target file.

  4. Release the `file_lock`.

  5. Return the return value.

- ***Write:*** `int write(int fd, const void *buffer, unsigned size)`

  1. Check the argument `buffer` is a valid pointer or not. If not, `exit(-1)` directly.

  2. Acquire `file_lock` to ensure the safety of critical section.

  3. Branch according specified `fd`:

     -  `if(fd == STDIN_FILENO)`, set return value to `-1`.

     - `else if(fd == STDOUT_FILENO)`, use `putbuf()` to write data on console and set return value to `size`.

     - `else`

       -- use function `find_des_by_fd()` to find the file descriptor whose `fd` is equal to the argument.

       -- check the descriptor we found is valid or not, and set the return value to `-1` if not.

       -- use `file_write()` to write the target file.

  4. Release the `file_lock`.

  5. Return the return value.



#### >>B4: Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel.  What is the least and the greatest possible number of inspections of the page table (e.g. calls to `pagedir_get_page()`) that might result?  What about for a system call that only copies 2 bytes of data?  Is there room for improvement in these numbers, and how much?

***Answer:***

- ***For a full page of data:***
  - The least number of inspections is 1. Since if `pagedir_get_page()` returns an address which is a page head, it implies that the full page is valid, so only one inspection is needed.
  - The greatest number of inspections is 4096. If the memory space we got is contiguous but the address we got from `pagedir_get_page()` is not a page head, we need to check twice(once for the address we got, once for the page end). However, if the memory space is not contiguous, we may need to check every address of this allocated space, so the greatest number is 4096.
- ***For a full page of data:***
  - The least number of inspections is 1. Since if the 2 bytes memory is contiguous and the first inspection of the first address tells us that it is more than two bytes far away from a page end, we can ensure that this 2 bytes space is valid.
  - The greatest number of inspections is 2. Since if the 2 bytes memory is contiguous, but the first inspection tells us that it is only one byte far away from a page end, then we need to do the second inspection. Or, if the 2 bytes space is not contiguous, we need to do two inspections whatever.



#### >>B5: Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

***Answer:***
Our `wait()` is implemented in `process_wait()`, it interacts with process termination frequently and necessarily.

In `process_Wait(tid_t child_tid)`: *(Note: the thread running `process_wait()` is parent thread)*

1. Check the validity of `child_tid`, if it is `TID_ERROR`, return `-1` directly, which means we are waiting for an invalid child thread.
2. If the `child_tid` is valid, we do the following operations:
   - Traverse current thread's `children_exit_code_list`, if we can find the corresponding `exit_code_list_element` of the child thread we want to wait for, that means this child thread has exited already. So return its `exit_code` directly.
   - If no corresponding `exit_code_list_element`, that means the child thread we want to wait for has not exited yet. Therefore, we traverse current thread's `children_t_list` to find this child thread. If we cannot find this child thread, that means we are waiting a thread which is not a child of current thread. So return `-1`.
   - If the thread we want to wait is indeed a child of current thread, we check whether it has been waited successfully or has been `THREAD_DYING`, if yes, that means we do not need to wait for this child thread, return `-1` directly.
   - Then, after several checks above, if we can ensure that the child thread should be waited, we acquire current thread's `loading_lock` and wait on `loading_cond` until the child thread exit. Since in our implementation of `process_exit()`, the child thread will `cond_broadcast ` its parent after removing itself from `all_list`. So the waiting condition in `process_wait()` is `while(find_thread_by_tid(child_tid) != NULL)`.
   - When the parent thread is signaled to wake up, we release the `loading_lock` and traverse current thread's `children_exit_code_list` again, because if child thread is freed before we can return from `process_wait()`, we can not return this child thread's `exit_code` which is in a block of freed memory. So we find this child thread's `exit_code_list_element` and retrieve its `exit_code`, so that we can return its exit status successfully.

The interaction between `wait()` and process termination occurs in the following three contexts:

1. At the beginning of `process_wait()` we traverse parent thread's `children_exit_code_list` in order to know whether child thread has exited already before `process_wait()` is called or not, both situations can happen in practice actually.
2. We choose to use condition variable to wait for child threads. If a process goes into termination step, we will remove it from `all_list`, and `cond_broadcast()` parent thread. Therefore, parent thread can not find child thread in `all_list`, waiting is done.
3. After waiting, we traverse parent thread's `children_exit_code_list` again because if we use child thread's pointer directly to retrieve its `exit_code`, we may get an invalid number because this child thread may have been freed before we can retrieve its `exit_code`. So we use its `exit_code_list_element` stored in `children_exit_code_list` to get the value.



#### >>B6: Any access to user program memory at a user-specified address can fail due to a bad pointer value.  Such accesses must cause the process to be terminated.  System calls are fraught with such accesses, e.g. a "write" system call requires reading the system call number from the user stack, then each of the call's three arguments, then an arbitrary amount of user memory, and any of these can fail at any point.  This poses a design and error-handling problem: how do you best avoid obscuring the primary function of code in a morass of error-handling?  Furthermore, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed?  In a few paragraphs, describe the strategy or strategies you adopted for managing these issues.  Give an example.

***Answer:***

- First, we implement a bad pointer checker called `bad_ptr()` in *"syscall.c"*. This checker will check three things:

  - Whether the given pointer is a `NULL` or not?
  - Whether the given pointer is an invalid user address or not?
  - Whether the given pointer is not mapped or not?

  Once at least one of the three conditions above is satisfied, we claim that the given pointer is invalid and cannot be used, should `exit(-1)`.

  Take system call `read(int fd, void *buffer, unsigned size)` as an example, we first check the interrupt frame's stack is valid or not before calling `read()`. In `read()`, we check the second argument `buffer` is valid or not. If everything is fine, we check every address we want to read into is valid or not, if the next address is invalid, we terminate reading in time. Therefore, every potential bad pointer will be checked before used, error can so that be avoided.

- Second, we implement exception handler in order to take action when an error is detected. The handling method is similar, we check the `fault_addr` is a valid pointer or not, and do the same thing as `exit(-1)` if it is indeed an invalid pointer.

  For example, the test case *"bad-jump-2"* wants to write to `0xc0000000`, which is an invalid address, it will cause an exception. Then, in handler, we check `0xc0000000` and find that this is indeed an invalid address. Therefore, we terminate the process just like `exit(-1)`.



### ----- Synchronization -----

#### >>B7: The "exec" system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading.  How does your code ensure this?  How is the load success/failure status passed back to the thread that calls "exec"?

***Answer:***

- In our implementation, we ensure this synchronization through condition variable and loading indicator `isloaded`.

  In `process_exec()`, after creating the child thread using `thread_create()`, we acquire current thread's `loading_lock` and `cond_wait()` on current thread's `loading_cond` if child thread's `isloaded` is still `0`. Concurrently, child thread is loading the file using the given function `load()`. If success, we change this child thread's `isloaded` to `1`, otherwise change that to `-1`. After updating the loading status, we `cond_broadcast()` to signal parent thread. 

  Therefore, regardless of the child thread load executable file successfully or not, the parent thread will wait till finding the loading status of its child thread has been changed. This ensures that the parent thread will return after the new executable has completed loading definitely.

- Actually, it has no need to pass load success/failure status back to parent thread in our implementation. Since we initialize `isloaded` to `0`, a child thread is loading only when its `isloaded` equals to `0`, any other value indicates the completion of its loading, success or failure. Therefore, all we want to know is whether the load status of child thread is `0` or not, rather than the exact value of it. So, we just need to check load status of child thread in `exec()`, if it is still `0`, wait for it. Otherwise, check whether the child thread has exited or not. If yes, retrieve this child thread's `tid` and pass it to `process_wait()`. If not, return `-1` if `isloaded != 1`, `0` otherwise.

  Thus, we did not record child thread's load status in its parent thread since there is no need to know the exact value of child thread's load status in `exec()` and also all later operations.



#### >>B8: Consider parent process P with child process C.  How do you ensure proper synchronization and avoid race conditions when P calls wait(C) before C exits?  After C exits?  How do you ensure that all resources are freed in each case?  How about when P terminates without waiting, before C exits?  After C exits?  Are there any special cases?

***Answer:***

We use a struct `exit_code_list_element` to record each thread's exit status and store it into each thread's parent's `children_exit_code_list`. Therefore, in whichever case, proper synchronization can be done. 

- ***P calls wait(C) before C exits:***

  This is a normal case. *P* will use the condition variable to wait *C* to exit, the elaborate explanation and description has been showed in question **B5**. Tricky point is, the child thread may exit before or after `wait(C)` returns. Therefore, after waiting, we need to traverse `children_exit_code_list` to find the child thread's `exit_code_list_element` and retrieve its `exit_code`, rather than use the pointer of the child thread directly. To free resources, when we find the C's `exit_code_list_element`, we remove it from P's `children_exit_code_list` and free it.

- ***P calls wait(C) after C exits:***

  This is a more abnormal case. To ensure correct synchronization in this case, we need to traverse `children_exit_code_list` to find the child thread's `exit_code_list_element` at the beginning of `wait(C)`. Therefore, if C has exited and been freed, we can know that and retrieve and return C's `exit_code` in time. Also,  when we find the C's `exit_code_list_element`, we remove it from P's `children_exit_code_list` and free it.

- ***P terminates without waiting, before C exits:***

  In `process_exit()`, we will traverse P's `children_exit_code_list` and free every entry in it. Also, we will traverse the `file_list` in kernel to close all files that is opened by P and free corresponding file descriptors, so no resources leak.

- ***P terminates after C exits:***

  This is a normal case, correct synchronization and resources free for the three cases above should ensure the correctness in this case.



### ----- Rationale -----

#### >>B9: Why did you choose to implement access to user memory from the kernel in the way that you did?

***Answer:***

We choose the method that verify the validity of a user-provided pointer first, then dereference it. Because this method is intuitive and simple to understand and implement.



#### >>B10: What advantages or disadvantages can you see to your design for file descriptors?

***Answer:***

As what has been explained in question **B2**, the advantage of our design of file descriptors is less space consuming in thread structure and global property of all opened files. However, the disadvantage is the threat that too many opened files may crash kernel memory space.



#### >>B11: The default `tid_t` to `pid_t` mapping is the identity mapping. If you changed it, what advantages are there to your approach?

***Answer:***

We did not change it.



## Survey Questions

Answering these questions is optional, but it will help us improve the course in future quarters.  Feel free to tell us anything you want--these questions are just to spur your thoughts.  You may also choose to respond anonymously in the course evaluations at the end of the quarter.

In your opinion, was this assignment, or any one of the three problems in it, too easy or too hard?  Did it take too long or too little time?

Did you find that working on a particular part of the assignment gave you greater insight into some aspect of OS design?

Is there some particular fact or hint we should give students in future quarters to help them solve the problems?  Conversely, did you find any of our guidance to be misleading?

Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects?

Any other comments?