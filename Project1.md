# CS130 Project 1: Threads

# Design Document

Group 31

- Yuqing Yao yaoyq@shanghaitech.edu.cn

- Chengfeng Zhao zhaochf@shanghaitech.edu.cn



## Task 1: Alarm Clock  *(implemented by Chengfeng Zhao)*

### ----- Data Structure -----

#### >>A1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

Added to struct thread:	

```c
/* Members for implementing timer_sleep() and timer_interrupt(). */
int64_t block_start;                /* Record the time the thread start to be blocked */
int64_t block_time;                 /* Record the time the thread should be blocked */
```

Initially, `block_start` and `block_time` are set to `INT64_MAX` and `0`, respectively.



### ----- Algorithms -----

#### >>A2: Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.

***Answer:***

**In a call to `timer_sleep()`:**

1. Get current timer ticks
2. Disable the interrupts
 3. Set the thread's `block_start` and `block_time` to current timer ticks and the parameter passed to `timer_sleep()`, respectively
 4. Block the thread
 5. Enable the interrupts

**The effects of timer interrupt handler:**

​	After `ticks` being increased by 1, use `thread_foreach()` and `unblock_or_not()` to unblock all blocked threads that have   been blocked enough long time. 

​	Here, `unblock_or_not()` is a function implemented by us to do the following things on every thread:

​		1 Judge if this thread has been blocked enough long time using current timer ticks, `block_start` and `block_time`

​		2 Unblock this thread if it is indeed a blocked thread and satisfies the judgment above

​		3 Reset this thread's `block_start` and `block_time` to `INT64_MAX` and `0` respectively	

​		4 Otherwise, `return` without doing anything

​		

#### >>A3: What steps are taken to minimize the amount of time spent in the timer interrupt handler?

***Answer:***

We did not take any step to try to minimize the amount of time spent in the timer interrupt handler in this part, but we do in the third part.

However, I think some steps like maintaining a new list which consists of all sleeping threads can reduce the time cost here. Since if we have a list consisting all sleeping threads, we can traverse this list only to check which thread can be unblocked. 



### ----- Synchronization -----

#### >>A4: How are race conditions avoided when multiple threads call timer_sleep() simultaneously?

***Answer:***

`timer_sleep()` requires interrupts enabled. So we disable interrupts before doing real "sleep" operations such, that makes race conditions avoided.



#### >>A5: How are race conditions avoided when a timer interrupt occurs during a call to timer_sleep()?

***Answer:***

The interrupts are disabled at the beginning of `timer_sleep()`.



### ----- Rationale -----

#### >>A6: Why did you choose this design?  In what ways is it superior to another design you considered?

***Answer:***

Since we don't want busy wait in `timer_sleep()`, we have to record the time a thread starting to be blocked and should be blocked in order for unblocking sleeping threads on time. Therefore, we use `bock_start` and `block_time` to record information about blocking. Then, because the timer ticks only change in `timer_interrupt()`, it's reasonable to do check and unblock qualified thread there.



## Priority Scheduling  *(implemented by Chengfeng Zhao)*

### ----- Data Structure -----

#### >>B1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

Added to struct lock:

```c
/* Member for putting struct lock into some list */
struct list_elem elem;       /* List element*/
/* Member for lock_release() */
int priority_representation; /* The representation(maximum) priority of the lock */
```

The `priority_representation` is initialized as `-1` in `lock_init()`.

------

Added to struct thread:

```c
/* Members for implementing lock_acquire() and lock_release() */
int ori_priority;                  /* Original Priority */
struct list lock_list;             /* All locks hold by the thread */
struct lock* lock_waiting;         /* The lock the thread is waiting */
```

Initially, `ori_priority` is set to the given priority, `lock_list` is initialized as an empty list, `lock_waiting` is set to `NULL`.



#### >>B2: Explain the data structure used to track priority donation. Use ASCII art to diagram a nested donation.  (Alternately, submit a .png file.)

***Answer:***

1. Explanation of the data structure used to track priority donation:
   - For struct lock:
     - When one single thread holds multiple locks, we use `struct list_elem elem` to link them as a list
     - We use `int priority_representation` to record the maximum priority among one lock's waiters
   - For struct thread:
     - We use `int ori_priority` to record the most initial priority of one thread
     - We use `struct list lock_list` to record all locks held by one thread
     - We use `struct lock* lock_waiting` to represent the lock that one thread is waiting for
   
2. Diagram nested donation:

   We simulate this situation:

   thread A acquires lock A, priority = 1  *(step1)*

   thread B acquires lock B  *(step2)*, then acquires lock A, priority = 2  *(step3)*

   thread C acquires lock B, priority = 3  *(step4)*

   thread A release lock A  *(step5)*

   thread B release lock B  *(step6)*

```
			thread A			|			  thread B  		    | 	 	         thread C
================================|===================================|========================================
           priority:1			|			 pririty:2              |               priority:3
         ori_priority:1         |          ori_priority:2           |             ori_priority:3
         lock_list:empty        |          lock_list:empty          |             lock_list:empty
        lock_waiting:NULL       |         lock_waiting:NULL         |            lock_waiting:NULL
                                |                                   |
step1:**************************|***********************************|****************************************
	       priority:1           |           priority:2              |               priority:3
         ori_priority:1         |          ori_priority:2           |             ori_priority:3	       
	     lock_list:lock A(-1)   |          lock_list:empty			|             lock_list:empty
		lock_waiting:NULL       |         lock_waiting:NULL         |            lock_waiting:NULL
step2:**************************|***********************************|****************************************
           priority:1           |           priority:2              |               priority:3
         ori_priority:1         |          ori_priority:2           |             ori_priority:3
         lock_list:lock A(-1)   |          lock_list:lock B(-1)     |             lock_list:empty
        lock_waiting:NULL       |         lock_waiting:NULL         |            lock_waiting:NULL
step3:**************************|***********************************|****************************************
           priority:2           |           priority:2              |               priority:3
         ori_priority:1         |          ori_priority:2           |             ori_priority:3
         lock_list:lock A(2)    |          lock_list:lock B(-1)     |             lock_list:empty
        lock_waiting:NULL       |         lock_waiting:lock A       |            lock_waiting:NULL
step4:**************************|***********************************|****************************************
           priority:3           |           priority:3              |               priority:3 
         ori_priority:1         |          ori_priority:2           |             ori_priority:3
         lock_list:lock A(3)    |          lock_list:lock B(3)      |             lock_list:empty
        lock_waiting:NULL       |         lock_waiting:lock A       |            lock_waiting:lock B
step5:**************************|***********************************|****************************************
		   priority:1           |           priority:3              |               priority:3 
         ori_priority:1         |          ori_priority:2           |             ori_priority:3
         lock_list:empty        |   lock_list:lock B(3),lock A(-1)  |             lock_list:empty
        lock_waiting:NULL       |         lock_waiting:NULL         |            lock_waiting:lock B
step6:**************************|***********************************|****************************************
           priority:1           |           priority:2              |               priority:3
         ori_priority:1         |          ori_priority:2           |             ori_priority:3
         lock_list:empty        |          lock_list:lock A(-1)     |             lock_list:lock B(-1)
        lock_waiting:NULL       |         lock_waiting:NULL         |            lock_waiting:NULL
=============================================================================================================
```



### ----- Algorithms -----

#### >>B3: How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?

***Answer:***

We ensure that through several changes:

- In function `next_thread_to_run()`, we always choose the thread which has the maximum priority in ready list to run.
- In function `sema_up()`, which is the key function for waking up waiting threads, we always choose the thread which has the maximum priority in semaphore's waiters.
- In function `cond_signal()`, we always choose to `sema_up` the semaphore whose waiters have the maximum priority among all waiters of this condition variable.

Therefore, for lock and semaphore, `sema_up()` always unblock the waiter which has the maximum priority first and then `next_thread_to_run` choose the maximum one in ready list first to run. And for conditional variable, `cond_signal()` always `sema_up` the semaphore which has the waiter that  has the maximum priority. So we can ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first.

***Note:***

Obviously, some new comparing functions are implemented by us and add to the system, they are:

```c
/* Function used to compare two thread's priority (in thread.h) */
int less_func(struct list_elem *e1, struct list_elem *e2, void* aux UNUSED);
/* Function used to compare two semaphore's waiters' priority (in sync.c) */
int sema_less_func(struct list_elem *e1, struct list_elem *e2, void *aux UNUSED);
```



#### >>B4: Describe the sequence of events when a call to lock_acquire() causes a priority donation.  How is nested donation handled?

***Answer:***

1. The sequence of events when a call to `lock_acquire()`:

   - Disable interrupts

   - **IF** this lock has no holder:
     - Push this lock into current thread's `lock_list`
   - **ELSE** :
     - If current thread's priority is larger than this lock's holder's, update lock's holder's priority and this lock's `priority_representation`
     - Initialize a `lock_iter` as current lock's holder's `lock_waiting`, and a `thread_iter` as current lock's holder, then do following things **till `lock_iter` is `NULL`** *or* **donation depth is larger than 8**:
       1. If `thread_iter`'s priority is larger than `lock_iter`'s holder's priority, update `lock_iter`'s holder's priority and `lock_iter`'s `priority_representation`
       2. Update `thread_iter` to `lock_iter`'s holder
     - Set current thread's `lock_waiting` as current lock
   - Call `sema_down()` to try to decrease this lock's semaphore or block itself to wait this lock
   - Set this lock's holder to current thread
   - Re-enable interrupts

2. In nested donation:

   Actually the handler of nested donation is just the loop described above. We iteratively find lock's holder's `lock_waiting` and update that lock's `priority_representation` and that lock's holder's `priority`. Finally, all lock's holder's priority in current donation chain and nest will be updated.



#### >>B5: Describe the sequence of events when lock_release() is called on a lock that a higher-priority thread is waiting for.

***Answer:***

- Remove this lock from current thread's `lock_list`	

- **IF** current thread's `lock_list` is empty now:

  - Reset current thread's priority to its `ori_priority`

- **ELSE** :

  - Traverse current thread's `lock_list`, and find the maximum `priority_representation` among all locks in this list, called `max_lock_priority`, tentatively

  - **IF** this `max_lock_priority` is `-1` which is an initialized value:

    ​	Reset current thread's priority to its `ori_priority`

  - **ELSE**:

    ​	Set current thread's priority to `max_lock_priority`

- Set this lock's holder to `NULL`

- Call `sema_up()` to release a semaphore value and **unblock the waiting thread which has the maximum priority**

- Check whether this lock's semaphore's waiter list is empty, reset this lock's `priority_representation` to `-1` if yes



### ----- Synchronization -----

#### >>B6: Describe a potential race in thread_set_priority() and explain how your implementation avoids it. Can you use a lock to avoid this race?

***Answer:***

1. `thread_set_priority()` and priority donation may have potential race. Since when a thread is being donated,  if donation is interrupted by `thread_set_priority()`, the priority donation may make no sense. This situation may also happen when a thread is releasing locks. Reversely, if a thread is to be set a new priority, priority donation may interrupt this process. Therefore, we disable interrupts at the beginning of `thread_set_priority()`, `lock_aquire()` and `lock_release()` to avoid this race.
2. I don't think we can use lock to avoid this race. If we want to use lock, we need to call `lock_acquire()` in `thread_set_priority()` to block itself. However, since our thread data structure does not include a member to record which lock is on the priority donation nest or chain, so lock can not be used to avoid this race in our implementation.



### ----- Rationale -----

#### >>B7: Why did you choose this design?  In what ways is it superior to another design you considered?

***Answer:***

We choose this design because it's straightforward and less time consuming.

The data `priority_representation` does not exist in our first design, since we make a mistake: we thought that when a new thread acquires a lock which has already held by another thread, the priority should not only donate to the lock's holder but also donate to the waiting threads before current thread(illustrated below). So we thought the head of waiter list always has the maximum priority, which can be found easily when operating `lock_release()`.

```
|==============|       acquire       |======|                 waiters
|  new thread  |   -------------->   | lock | <-------------+---------+
|==============|                     |======|               | thread1 | priority(5)
  priority(10)                   holder priority(6)         +---------+        
                                                            | thread2 | priority(3)
													        +---------+
								||
								||
								||
								||	priority donation
								||
								||
							\	||    /
							  \     /
							    \ /

|==============|       acquire       |======|         waiters
|  new thread  |   -------------->   | lock | <-----+---------+
|==============|                     |======|       | thread1 | priority(10)
  priority(10)                  holder priority(10) +---------+        
                                                    | thread2 | priority(10)
													+---------+
```

However, the correct donation is only donate to the lock's holder and donate in a nest and chain(illustrated below).

```
|==============|       acquire       |======|                 waiters
|  new thread  |   -------------->   | lock | <-------------+---------+
|==============|                     |======|               | thread1 | priority(5)
  priority(10)                   holder priority(6)         +---------+        
                                                            | thread2 | priority(3)
													        +---------+
								||
								||
								||
								||	priority donation
								||
								||
							\	||    /
							  \     /
							    \ /

|==============|       acquire       |======|         waiters
|  new thread  |   -------------->   | lock | <-----+---------+
|==============|                     |======|       | thread1 | priority(5)
  priority(10)                  holder priority(10) +---------+        
                                                    | thread2 | priority(3)
													+---------+
```

Therefore, when we do `lock_release()`, if we do not record the maximum priority among the waiters of the lock, we need to traverse the waiter list and find that, which is time-consuming. So we introduce `priority_representation` to record that to find which priority the thread should be reduced to quickly and easily.

Other implementations are straightforward and concept-driven.



## Advanced Scheduler  *(implemented by Chengfeng Zhao)*

### ----- Data Structures -----

#### >>C1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.

***Answer:***

A file "fixed_point.h" is added.

------

Added to struct thread:

```c
int niceness;                       /* Nice value of the thread: [-20, 20] */
int64_t recent_cpu;                 /* Recent CPU of the thread */
```

`niceness` and `recent_cpu` are both be either initialized as `0` or inherit that from parent thread. 

------

Added to thread.c:

```c
static int64_t load_avg;			/* The load average value of this system */
```

`load_avg` is initialized as `0`.



### ----- Algorithms -----

#### >>C2: Suppose threads A, B, and C have nice values 0, 1, and 2.  Each has a recent_cpu value of 0.  Fill in the table below showing the scheduling decision and the priority and recent_cpu values for each thread after each given number of timer ticks:

***Answer:***

```
            |  recent_cpu  |   priority   |   			   |
------------+--------------+--------------+----------------+
timer ticks |  A |  B |  C |  A |  B |  C |  thread to run |
------------+----+----+----+----+----+----+----------------+
 	   0    |  0 |  1 |  2 | 63 | 61 | 59 |       A        |
 	   4    |  4 |	1 |  2 | 62 | 61 | 59 |		  A	       |
 	   8    |  8 |	1 |  2 | 61	| 61 | 59 |		  B        |
	  12    |  8 |	5 |	 2 | 61 | 60 | 59 |       A        |
	  16    | 12 |	5 |	 2 | 60	| 60 | 59 |		  B	       |
	  20    | 12 |	9 |	 2 | 60 | 59 | 59 |		  A	       |
	  24    | 16 |	9 |	 2 | 59	| 59 | 59 |		  C        |
	  28    | 16 |	9 |	 6 | 59	| 59 | 58 |       A        |
	  32    | 20 |  9 |	 6 | 58	| 59 | 58 |       B        |
	  36    | 20 | 13 |  6 | 58 | 58 | 58 |		  C        |
```



#### >>C3: Did any ambiguities in the scheduler specification make values in the table uncertain?  If so, what rule did you use to resolve them?  Does this match the behavior of your scheduler?

***Answer:***

1. Yes, ambiguities exist in `recent_cpu`, depending on different scheduler specifications.
2. The rule we use is increasing the `recent_cpu` of currently running thread by 1 per tick before doing all the other things. Then we recalculate currently running thread's priority per 4 ticks, and update `recent_cpu`, priority and `load_avg` globally per 100 ticks.



#### >>C4: How is the way you divided the cost of scheduling between code inside and outside interrupt context likely to affect performance?

***Answer:***

We found that the performance will reduce if cost of scheduling inside interrupt context is high.

First, theoretically speaking, if the cost inside interrupt context is high, the interrupt will cost most of time which should originally be the running time of thread. Therefore, at a same time interval(e.g. 100 seconds), the time cost inside interrupt context higher, the thread running slower, and this will eventually lead to thread's `recent_cpu` higher and priority lower. 

Second is our practice. At first, we do this part by totally following what is said in the official document. However, we found that the `load_avg` is somehow bigger than expectations. And through tough efforts, we found that we don't need to really recalculate the priority of all threads every 4 ticks, since priority will change only when `recent_cpu` is changed, which will happen every 100 ticks. So we only need to do priority recalculation globally only once per 100 ticks and do that for currently running thread once per 4 ticks. After improving this problem, the `load_avg` reduced exactly.

So, in summary, from both theory and practice perspective, we found that if the cost inside interrupt context is high, the performance of the system is low.   



### ----- Rationale -----

#### >>C5: Briefly critique your design, pointing out advantages and disadvantages in your design choices.  If you were to have extra time to work on this part of the project, how might you choose to refine or improve your design?

***Answer:***

1. Brief comment:

   Our design is direct and simple, also good space complexity and time complexity. The disadvantage of our design is that our time complexity is not so good. We implement a single queue rather than 64 queues, which reduce the space complexity. And for the single queue(ready_list), we do not sort it, which will cost ***O(n)*** time for selecting the thread next to run. This is better than sorted implementation, since every 4 ticks it will cost ***O(nlogn)*** to re-sort the ready list.

2. Refinement and improvement:

   If I were to have extra time, I will implement 64 queues. Since it will only cost ***O(n)*** time per second to rearrange all the threads in the element, which has better time complexity.



## Survey Questions

Answering these questions is optional, but it will help us improve the course in future quarters.  Feel free to tell us anything you want--these questions are just to spur your thoughts.  You may also choose to respond anonymously in the course evaluations at the end of the quarter.

In your opinion, was this assignment, or any one of the three problems in it, too easy or too hard?  Did it take too long or too little time?

Did you find that working on a particular part of the assignment gave you greater insight into some aspect of OS design?

Is there some particular fact or hint we should give students in future quarters to help them solve the problems?  Conversely, did you find any of our guidance to be misleading?

Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects?

Any other comments?

