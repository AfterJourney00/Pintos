# CS130 Project 3: Virtual Memory

# Design Document

*Group 31*

- Yuqing Yao yaoyq@shanghaitech.edu.cn
  - swap space
- Chengfeng Zhao zhaochf@shanghaitech.edu.cn
  - supplemental page table, frame table, memory mapped files


-----



## Part1: Page Table Management

### ----- Data Structures -----

#### >>A1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

*New structures for supplemental page table:*

```c
/* Three types of supplemental pte. One pte can only has one type */
enum supp_type
{
  LAZY_LOAD,                      /* Only supplemental page table entry exists, for lazy load */
  CO_EXIST,                       /* Both supplemental pte and real page exist */
  EVICTED,                        /* Only supplemental page table entry exists, for swap */
};

/* Structure of supplemental page table entry */
struct supp_page
{
  enum supp_type type;            /* Indicate the type of this supplemental pte */
  uint8_t* user_vaddr;            /* The user virtual address corresponding to this page entry*/
  struct file* file_in_this_page; /* Record the owner of this page */
  off_t load_offset;              /* Record the loading start */
  uint32_t read_bytes;            /* Record how many bytes to read */
  uint32_t zero_bytes;            /* Record how many bytes to set to 0 */
  bool writable;                  /* Record the writable attribute of this page */
  bool fake_page;                 /* Inidicate this page is a fake page or real */ 
  size_t swap_idx;                /* Record which swap slot evicted to(only for type:EVICTED) */  
  
  struct hash_elem h_elem;        /* Element for hash table */
};
```

------

*New variables added to struct thread:*

```c
struct hash page_table;             /* Page table of this thread(process) */
uint8_t* sp;                        /* Record the stack pointer of the thread */
```



### ----- Algorithms -----

#### >>A2: In a few paragraphs, describe your code for accessing the data stored in the SPT about a given page.

***Answer:***

There is a function `find_fake_pte()` implemented by ourselves works for it.

Given a page*(actually a mapped user virtual address)*, it will be regarded as a hash key of the supplemental page table*(the supplemental page table is implemented as a hash table)*. Then, use with this hash key, we create a new frame table entry object which owns this hash key. After that, we use this new frame table entry object and provided function `hash_find()` to find the corresponding supplemental page table entry.

There are two possible cases of this finding:

1.  The page we want has already owned its supplemental page table entry. In this case, the supplemental page table entry is found and returned, and we can access all the data about the page that we are interested in.
2. The page we want does not have its supplemental page table entry. Well, in our implementation, this case can occur when:
   - A bad access, should kill the process.
   - A request for stack growth, we create a frame and a corresponding SPT for this page.
   - Evict a frame which has no corresponding SPT, we create one and store some essential information.

Therefore, given any page, we can know its information through SPT.



#### >>A3: How does your code coordinate accessed and dirty bits between kernel and user virtual addresses that alias a single frame, or alternatively how do you avoid the issue?

***Answer:***

We avoid this issue by focusing only on user virtual address.

For example, all accesses in our implementation is through user virtual address and all checking about *dirty or not* or *accessed or not* is about user virtual address using `pagedir_is_dirty(pagedir, uaddr)` and `pagedir_is_accessed(pagedir, uaddr)`.



### ----- Synchronization -----

#### >>A4: When two user processes both need a new frame at the same time, how are races avoided?

***Answer:***

Lock can help us avoid the races. We assign a specific `frame_lock` for frame table, any operation on frame table*(including allocation, push_back, remove and modification on frame table entry)* should acquire this `frame_lock` and release it after operation. Therefore, we can ensure that only one process can request a new frame at a time.



### ----- Rationale -----

#### >>A5: Why did you choose the data structure(s) that you did for representing virtual-to-physical mappings?

***Answer:***

- Since supplemental page is actually an additional information block for a page, so it can have 3 different states:

  - Only supplemental page but no real page, no mapping relation yet. This is `LAZY_LOAD`
  - Both supplemental page and real page exist, mapping relation has existed. This is `CO_EXIST`
  - Real page is evicted, mapping relation is to be cleared. This is `EVICTED`

  Therefore, we use these three types to record the state of a real page.

- The `sp` added to thread struct is because we need to retrieve the user stack pointer when a page fault occurs in kernel mode.

- For other state variables, since they are necessary for eviction, lazy load, reclaimation and other operations, we have to add them into our data structure to implement SPT.



## Part2: Paging to and from Disk

### ----- Data Structures -----

#### >>B1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

*New structures for frame table:*

```c
#define LATEST_CREATE_TIME 0xefffffff

/* Frame table, every entry is a frame */
struct list frame_table;
struct lock frame_lock;

/* Frame table entry */
struct frame{
  uint8_t *frame_base;          /* Base address */
  struct thread* allocator;     /* The frame's allocator */
  uint32_t *pte;                /* Record the corresponding page table entry */
  int64_t created_time;         /* Record the time this frame is created */
  void* user_vaddr;             /* Record the corresponding user virtual address */
  bool locked;                  /* Indicate whether the frame can be evicted */
  struct list_elem elem;        /* Element for list */
};
```

------

*New structures for swap space:*

```c
#define SIZE_PER_SECTOR 512
#define SECTORS_PER_PAGE 8

static struct block* block_device;        /* The block device */
static struct bitmap* swap_space_map;     /* The bit map to record availability of swap space */
struct lock swap_lock;                    /* Synchronization variable for swap space */
```



### ----- Algorithms -----

#### >>B2: When a frame is required but none is free, some frame must be evicted.  Describe your code for choosing a frame to evict.

***Answer:***

The policy we use in eviction is ***LRC(Least Recently Created)***. That is, we always choose the frame that is created earliest to evict.

The whole process is:

- Acquire the `frame_lock` to do synchronization.
- Initialize a variable `create_time` to `0xefffffff`, which represents the possibly latest created time.
- Traverse the frame table, compare each frame's created time with the `create_time`:
  - If the frame is not pinned and has earlier created time, regard it as the possible victim frame and update `create_time`
  - Else, do nothing and continue the loop.
- Then we can get the victim frame, remove it from the frame table and set it to pinned.
- return the victim frame.



#### >>B3: When a process P obtains a frame that was previously used by a process Q, how do you adjust the page table (and any other data structures) to reflect the frame Q no longer has?

***Answer:***

1. If the frame Q now still exists, it is impossible for process P to have the same frame because no sharing in our implementation. All the frames of P are different frames with Q's.
2. If the frame Q has not existed or process Q has exited or killed, all the frames belonging to process Q will be freed, frames allocated for process P are all new frames. Even though some frame of P is a physically same frame with someone of Q previously, we have an `allocator` variable in `frame` struct, which records what process allocates it.

Therefore, no sharing ensures no ambiguity and no inconsistency, and `allocator` variable in `frame` struct clarifies it.



#### >>B4: Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the page that faulted.

***Answer:***

The heuristic follows the verification rule described in official document.

In our implementation, a page fault which requests extra stack should satisfy following conditions in page fault handler:

1. We can not find the corresponding supplemental page table entry in SPT.
2. The `fault_addr` is higher than `0x08048000`.
3. The `fault_addr` is higher than `esp - 32`.

If these conditions are all satisfied, it will be determined as a request for stack growth.



### ----- Synchronization -----

#### >>B5: Explain the basics of your VM synchronization design.  In particular, explain how it prevents deadlock. (Refer to the textbook for an explanation of the necessary conditions for deadlock.)

***Answer:***

The four necessary condition for deadlock is:

1. **Mutually exclusive resources:** There are some resources that can only be held by one process at a time.
2. **No preemption:** There is no preemption mechanism to preempt resources from some process externally.
3. **Waiting while holding:** A process is waiting for some resources while holding some resouces.
4. **Circular wait:** Process A is waiting for process B, which is waiting for process A.

Well, we choose to avoid deadlock by avoiding **Waiting while holding**. In our implementation of VM part, there are two global locks:

- `frame_lock`: synchronize operations on frame.
- `swap_lock`: synchronize operations on swap space.

When we want to evict or reclaim frame, the swap operations are embedded into frame operations. If we do not design modularity, it is likely to wait `swap_lock` while holding `frame_lock`. Therefore, we take measure of modularity, acquire and release the lock inside each operation function, which makes **Waiting while holding** impossible, and so that make deadlock impossible.



#### >>B6: A page fault in process P can cause another process Q's frame to be evicted.  How do you ensure that Q cannot access or modify the page during the eviction process?  How do you avoid a race between P evicting Q's frame and Q faulting the page back in?

***Answer:***

1. As mentioned before, all operations on frame should acquire the `frame_lock`. If process P is evicting a frame belonging to process Q, P must have owned the `frame_lock` already and Q must wait for the eviction to finish. So, during the eviction process, process Q cannot access or modify the page.
2. Reclaimation also needs `frame_lock`. If P acquires the `frame_lock` first, Q will wait until the eviction finish, and then do reclaimation. If Q acquires the lock first, P will wait until the reclaimation finish, and then do eviction.



#### >>B7: Suppose a page fault in process P causes a page to be read from the file system or swap.  How do you ensure that a second process Q cannot interfere by e.g. attempting to evict the frame while it is still being read in?

***Answer:***

There is a state variable `locked` in `frame` structure to represent whether this frame can be chosen to be evicted or not. When a frame is to be reclaimed, we will set the frame's `locked` to be true. Since this operation is protected by `frame_lock`, when Q wants to choose some frame to evict, it will see that this to be reclaimed frame is locked and cannot be chosen to be evicted. Therefore, Q won't interfere the reclaimation of the frame P is reclaiming.



#### >>B8: Explain how you handle access to paged-out pages that occur during system calls. Do you use page faults to bring in pages (as in user programs), or do you have a mechanism for "locking" frames into physical memory, or do you use some other design?  How do you gracefully handle attempted accesses to invalid virtual addresses?

***Answer:***

In system calls, such case can only occur in `read()`. 

In our implementation, we just create a internal page fault handler in `read()`. The pages are checked before get into the real functionality of the system call. If it’s in paged out, it will be brought back just as in user programs. Then the corresponding frames to the pages will be locked, so that other process never choose this frame as a victim frame to evict.

For invalid virtual addresses*(NULL, kernel virtual address, not a request for stack growth, etc.)*, they will be checked at the beginning of `read()`, and the process will be terminated.



### ----- Rationale -----

#### >>B9: A single lock for the whole VM system would make synchronization easy, but limit parallelism.  On the other hand, using many locks complicates synchronization and raises the possibility for deadlock but allows for high parallelism.  Explain where your design falls along this continuum and why you chose to design it this way.

***Answer:***

1. First, I think use a single lock for the whole VM is a good idea. Since swap space and frame table are all global, and almost all the operations among them are embedded to each other, use a single lock is equivalent to *"acquire-all, release-all"* approach, which can avoid deadlock efficiently.
2. However, use a single lock for the whole VM uses no advantage of modularity. Pintos is just a very slight-wight operating system, for those real operating systems, the whole virtual memory space cannot use a single lock to synchronize, which is too coarse-grained. Also, a single lock can make the critical section very big, which can reduce the performance greatly. 

Therefore, for the philosophy of modularity and better performance, we choose use two different global locks to synchronize our VM.



## Part3: Memory Mapped Files

### ----- Data Structures -----

#### >>C1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

*New structure for memory mapped files:*

```c
typedef int mapid_t;

struct mmap_file_des
{
   mapid_t id;                          /* Id of this memory-mapped file */
   struct file* file_ptr;               /* Pointer of this memory-mapped file */
   void* mapped_addr;                   /* Start address of the mapped memory */
   int length;                          /* Valid length of the mapped memory */      
   struct list_elem elem;               /* Element for list */
};
```

------

*New variables added to struct thread:*

```c
struct list mmap_file_list;             /* Memory-maped file list */
```



### ----- Algorithms -----

#### >>C2: Describe how memory mapped files integrate into your virtual memory subsystem.  Explain how the page fault and eviction processes differ between swap pages and other pages.

***Answer:***

- ***How memory mapped files integrate into your virtual memory subsystem:***

  - Every process has a memory mapped files list, which records those files mapped in this process.
  - Processes can use corresponding system call to operate memory mapped files.

- ***How the page fault and eviction processes differ between swap pages and other pages:***

  swap page is a page that is chosen by some process to evict, and should be written into swap space through block device. Other pages may be a non-present page, which may should be loaded*(lazy load, memory mapped files)* or should be retrieved from swap space to memory.

- 

  



#### >>C3: Explain how you determine whether a new file mapping overlaps any existing segment.

***Answer:***

1. Find the file we want map and get its size.
2. Take `PG_SIZE` as the stride and go through 0 to the size of the file:
   - Add this offset to the user virtual address that we want to map to get an address
   - Use function `find_fake_pte()` and this address to check whether there has existed some page mapped to the same address.
     - If so, that means there are some overlapping segment.
     - Otherwise, no overlapping, and the map operation can continue successfully.



### ----- Rationale -----

#### >>C4: Mappings created with "mmap" have similar semantics to those of data demand-paged from executables, except that "mmap" mappings are written back to their original files, not to swap.  This implies that much of their implementation can be shared.  Explain why your implementation either does or does not share much of the code for the two situations.

***Answer:***

In our implementation, we share much of code of the two situations.

This is a natural idea, since the `mmap` operation works just like lazy load, which can use all the code used by the segment load part of a process. Therefore, sharing the code makes programmer life easy and make the code not redundant.



## Survey Questions

Answering these questions is optional, but it will help us improve the course in future quarters.  Feel free to tell us anything you want--these questions are just to spur your thoughts.  You may also choose to respond anonymously in the course evaluations at the end of the quarter.

In your opinion, was this assignment, or any one of the three problems in it, too easy or too hard?  Did it take too long or too little time?

Did you find that working on a particular part of the assignment gave you greater insight into some aspect of OS design?

Is there some particular fact or hint we should give students in future quarters to help them solve the problems?  Conversely, did you find any of our guidance to be misleading?

Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects?

Any other comments?