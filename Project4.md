# CS130 Project 4: File Systems

# Design Document

*Group 31*

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Chengfeng Zhao zhaochf@shanghaitech.edu.cn

----

## Part 1: Indexed and Extensible Files

### ---- Data Structures ----

#### >> A1: Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or `enumeration`.  Identify the purpose of each in 25 words or less.

***Answer:***

*Some macros added in inode.c:*

```C
#define FIRST_LAYER_SECTORS 123
#define SECTORS_PER_SECTOR 128
#define SECOND_LAYER_SECTORS SECTORS_PER_SECTOR
```

------

*New structure for inode_disk:*

```C
struct inode_disk
{
  block_sector_t direct_sectors[FIRST_LAYER_SECTORS];  /* Direct sector indices */
  block_sector_t indirect_sector_idx;                  /* Indirect sector indices */
  block_sector_t doubly_indirect_sector_idx;           /* Doubly indirect sector indices */

  bool is_dir;                        /* Record this file is a directory or not */
  off_t length;                       /* File size in bytes. */
  unsigned magic;                     /* Magic number. */
};
```



#### >> A2: What is the maximum size of a file supported by your inode structure?  Show your work.

***Answer:***

The maximum size of a file can be computed:

- How many block sectors our file system has:

$$
123 + 1\times 128 + 1\times 128\times 128 = 16635\space (sectors)
$$

- Therefore, the maximum size of a file is:
  $$
  16635\times 512 = 8517120B\approx 8MB
  $$
  



### ---- Synchronization ----

#### >> A3: Explain how your code avoids a race if two processes attempt to extend a file at the same time.

***Answer:***

We use the `file_lock` to do the protection and synchronization. This lock is introduced to our system in proj2, and it provides prevention of races for all the system calls about file system.

In detail, since a file should be extended only when user wants to write this file at some position past the EOF. This means the file extension operation can only be operated in a `write()` syscall. At the beginning of the `write` syscall, we acquire the `file_lock` and do all the writing operation atomically. Therefore, the race of two processes that attempt to extend a file at the same time can be avoided definitely because only one process can write to file at a time.



#### >> A4: Suppose processes A and B both have file F open, both positioned at end-of-file.  If A reads and B writes F at the same time, A may read all, part, or none of what B writes.  However, A may not read data other than what B writes, e.g. if B writes nonzero data, A is not allowed to see all zeros.  Explain how your code avoids this race.

***Answer:***

In our implementation, the only two possible situations can occur are:

1. Process A acquire the `file_lock` first and do its reading operation atomically, then process B can do its writing.
2. Process B acquire the `file_lock` first and do its writing operation atomically, then process A can do its reading.

This is because the synchronization is done globally in system calls, rather than file specifically. That is to say, as having been said before, as long as processes do their file system related operations through system calls, only one process can access to the file system at a time. Therefore, we can avoid such races.



#### >> A5: Explain how your synchronization design provides "fairness". File access is "fair" if readers cannot indefinitely block writers or vice versa.  That is, many processes reading from a file cannot prevent forever another process from writing the file, and many processes writing to a file cannot prevent another process forever from reading the file.

***Answer:***

We did not consider "fairness" when do our synchronization design.

If there are infinite number of processes request writing before a process which requests a reading, this reading process may have to wait for a long long time in our implementation. However, there are no infinite number of processes in practice, a process which want to read files can eventually finish its job.

But taking "fairness" into consideration can improve the file system's performance, definitely.



### ---- Rationale ----

#### >> A6: Is your inode structure a multilevel index?  If so, why did you choose this particular combination of direct, indirect, and doubly indirect blocks?  If not, why did you choose an alternative inode structure, and what advantages and disadvantages does your structure have, compared to a multilevel index?

***Answer:***

Yes, our inode structure is a multilevel index.

We choose this particular combination because:

1. There is a size limit of ***512B*** for inode structure. Therefore we need to be careful to design the layout of our multilevel index. Finally we choose 123 direct index, 1 indirect index and 1 doubly indirect index, which satisfies the limit constraint combined with other variables in this structure.
2. Asymmetric multilevel index structure is good for both small files and large files. Since small files can be stored with only direct index and so that can be accessed very quickly. For large files, multilevel index can support sparse file allocation and larger size of file.
3. The exact number of direct blocks, indirect blocks and doubly indirect blocks we choose is due to the recommendation of official documentation.



## Part 2: Subdirectories

### ---- Data Structures ----

#### >> B1: Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or `enumeration`.  Identify the purpose of each in 25 words or less.

***Answer:***

*Added to struct thread:*

```c
struct dir* cwd;                    /* Record the current working directory */
```



### ---- Algorithms ----

#### >> B2: Describe your code for traversing a user-specified path.  How do traversals of absolute and relative paths differ?

***Answer:***

The specific path traversal follows the following steps:

1. We split the whole specific path into two parts: *directory part* and *file part*. We traverse the given path character by character from tail to head until we find the first *'/'*. The string located at the right hand side of the *'/'* is the file name and the string at the left hand side is the directory name.
2. Then the directory name will be split layer by layer:

   - If the directory name is a empty name, it indicates that the specific path is a relative path and we should open the fiile in the cwd directly. *[Exception: if both the directory name and file name are all empty, this is an invalid path]*
   - Otherwise, we check whether the first character of the directory name is a *'/'*:
     - If so, it means the given path is an absolute path, and we should traverse the path starting from root directory.
     - Otherwise, it means we should start from cwd.
   - After knowing the starting directory of traversal, we lookup sub-directory to check whether the path given is valid. The helper function we use is`strtok_r()`. If the path is valid and correct, we can traverse to the deepest directory which contains the target file given by the path.

Therefore, any user-specific path, absolute or relative, will be split and traversed correctly.



### ---- Synchronization ----

#### >> B4: How do you prevent races on directory entries?  For example, only one of two simultaneous attempts to remove a single file should succeed, as should only one of two simultaneous attempts to create a file with the same name, and so on.

***Answer:***

We use the `file_lock` to do the protection and synchronization. This lock is introduced to our system in proj2, and it provides prevention of races for all the system calls about file system.

Since any operation about directory, whatever `create()` or `remove()`, should be called through system call, the `file_lock` can guarantee that only one process can make one of these operations about directory. Therefore, we can definitely prevent such races.



#### >> B5: Does your implementation allow a directory to be removed if it is open by a process or if it is in use as a process's current working directory?  If so, what happens to that process's future file system operations?  If not, how do you prevent it?

***Answer:***

Yes, cwd can be removed in our implementation.

To handle the situation that a process want to open or read directory of its cwd when the cwd has been removed, we will check whether the current process's cwd is removed:

- If the cwd is removed, we regard this directory operation as a failure.
- Otherwise, we process the operation as normal.



### ---- Rationale ----

#### >> B6: Explain why you chose to represent the current directory of a process the way you did.

***Answer:***

Since cwd is a per-process concept, we need to record it in our process structure like *PCB* (however, the processes in Pintos are all single-threaded process, so we record it directly in struct thread).

We need to record it like that also because it provides necessary information about current working directory when we want to open a relative path or change working directory.



## Part 3: Buffer Cache

### ---- Data Structures ----

#### >> C1: Copy here the declaration of each new or changed `struct` or struct member, global or static variable, `typedef`, or `enumeration`.  Identify the purpose of each in 25 words or less.

***Answer:***

*New structures for buffer cache:*

```C
#define CACHE_SIZE 64

struct cache_line
{
  bool valid_bit;                   /* Only use cache line with true valid bit */
  bool dirty_bit;                   /* Only write back to disk with true dirty bit */
  bool available;                   /* Indicate whether this cache line is available */
  int64_t accessed_time;            /* Use LRU cache eviction policy */

  block_sector_t sector_idx;        /* Record which sector should this cache line write back */
  char* buffer;                     /* Content of this cache line(512 bytes) */
};

/* The whole cache, an array of cache lines */
struct cache_line cache[CACHE_SIZE];

/* Synchronization variable for cache system */
struct lock cache_lock;
```



### ---- Algorithms ----

#### >> C2: Describe how your cache replacement algorithm chooses a cache block to evict.

***Answer:***

The replacement algorithm we choose to use is *LRU(Least Recently Used)*. We add a member variable `accessed_time` in `cache_line` structure to record the time point of the most recent access. Then, if a new cache miss occurs and we need to load a block of data in cache when our buffer cache is full, we traverse all the 64 cache lines and select the one whose `accessed_time` is the earliest to evict.



#### >> C3: Describe your implementation of write-behind.

***Answer:***

We implement ***write-behind*** by writing cache line back to disk only when eviction occurs or the whole file system is shutdown.

1. When the victim cache line we choose to evict is a dirty cache line, we write it back to disk. At that time, many versions of update may have occurred on that cache line.
2. When `filesys_done()` is called at the end of the file system, we traverse all the 64 cache lines and write back if it is dirty.
3. In other cases, we just write updates to the buffer cache but not to the disk.



#### >> C4: Describe your implementation of read-ahead.

***Answer:***

We didn't implement read-ahead.



### ---- Synchronization ----

#### >> C5: When one process is actively reading or writing data in a buffer cache block, how are other processes prevented from evicting that block?

***Answer:***

We use a `cache_lock` to do the protection and synchronization.

In our implementation, the only interface provided by buffer cache is `cache_do()`, which can be used to do cache read or cache write. In the beginning of `cache_do()`, we acquire the `cache_lock` and do following operations atomically:

- Check this is a cache hit or cache miss:
  - If this is a cache hit, access the target cache line.
  - Otherwise, do following checks.
- Check whether any free cache line is available or not:
  - If there is some cache line is free and available, load the block of data we want to this cache line.
  - Otherwise, do following checks.
- Use *LRU* algorithm to choose a cache line to evict.
- Load the block of data we want from disk to cache.
- Do read or write.

After all the operations above are finished, we release the `cache_lock`.

Therefore, when one process is actively reading or writing data in a buffer cache block, it must be holding the `cache_lock`, and so that no other processes can access any cache block.



#### >> C6: During the eviction of a block from the cache, how are other processes prevented from attempting to access the block?

***Answer:***

As our answer of ***question C5***, we implement the whole ***"check cache hit or miss -> find a free cache line -> choose one cache line to evict -> do eviction -> do read or write"*** pipeline as one single atomic operation. Therefore, only one cache access can be operated and processed at a single time, and so that no other processes can attempt to access the cache block during an eviction.



### ---- Rationale ----

#### >> C7: Describe a file workload likely to benefit from buffer caching, and workloads likely to benefit from read-ahead and write-behind.

***Answer:***

1. *Describe a file workload likely to benefit from buffer caching:*

   Since we typically access file sequentially or within a limited region, reading or writing. Suppose that we now want to read and write 512B of a file, sequentially. If without buffer cache, we need to goto disk 512 times, which is very slow. However, if we can cache the 512B of the target file when the first access to the disk occurs, we can only do reading or writing to cache at the remaining 511 accesses, which can be much faster than before.

2. *Workloads likely to benefit from read-ahead and write-behind.*

   - For *read-ahead*, it supports a larger cache region and avoid disk rotational delay. For example, when a process wants to read 513B of a file, it will miss the first access and hit the remaining 511 accesses. Then, if without *read-ahead*, not only will it have a second cache miss and disk access, but also there may be some rotational delay, which can cause accessing error. However, with *read-ahead*, only one cache miss can occur and the problem of rotational delay can be solved.
   - For *write-behind*, since no need to flush cache to disk for every write, the performance will be improved and the file workloads are likely to benefit from it, if the system is reliable enough.



## Survey Questions

Answering these questions is optional, but it will help us improve the course in future quarters.  Feel free to tell us anything you want--these questions are just to spur your thoughts.  You may also choose to respond anonymously in the course evaluations at the end of the quarter.

In your opinion, was this assignment, or any one of the three problems in it, too easy or too hard?  Did it take too long or too little time?

Did you find that working on a particular part of the assignment gave you greater insight into some aspect of OS design?

Is there some particular fact or hint we should give students in future quarters to help them solve the problems?  Conversely, did you find any of our guidance to be misleading?

Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects?

Any other comments?