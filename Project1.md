# Project 1: Threads

Group 31

- Yuqing Yao yaoyq@shanghaitech.edu.cn

- Chengfeng Zhao zhaochf@shanghaitech.edu.cn

## Task 1: Alarm Clock  *(implemented by Chengfeng Zhao)*

### Data Structure

#### A1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.







### Algorithms

#### A2: Briefly describe what happens in a call to `timer_sleep()`, including the effects of the timer interrupt handler.



#### A3: What steps are taken to minimize the amount of time spent in the timer interrupt handler?



### Synchronization

#### A4: How are race conditions avoided when multiple threads call timer_sleep() simultaneously?



#### A5: How are race conditions avoided when a timer interrupt occurs during a call to timer_sleep()?



### Rationale

#### A6: Why did you choose this design?  In what ways is it superior to another design you considered?



## Priority Scheduling  *(implemented by Chengfeng Zhao)*

### Data Structure

#### B1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.



#### B2: Explain the data structure used to track priority donation. Use ASCII art to diagram a nested donation.  (Alternately, submit a .png file.)



### Algorithms

#### B3: How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?



#### B4: Describe the sequence of events when a call to lock_acquire() causes a priority donation.  How is nested donation handled?



#### B5: Describe the sequence of events when lock_release() is called on a lock that a higher-priority thread is waiting for.



### Synchronization

#### B6: Describe a potential race in thread_set_priority() and explain how your implementation avoids it. Can you use a lock to avoid this race?



### Rationale

#### B7: Why did you choose this design?  In what ways is it superior to another design you considered?



## Advanced Scheduler  *(implemented by Chengfeng Zhao)*

### Data Structures

#### C1: Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or enumeration.  Identify the purpose of each in 25 words or less.



### Algorithms

#### C2: Suppose threads A, B, and C have nice values 0, 1, and 2.  Each has a recent_cpu value of 0.  Fill in the table below showing the scheduling decision and the priority and recent_cpu values for each thread after each given number of timer ticks:

```
timer  recent_cpu    priority   thread
ticks   A   B   C   A   B   C   to run
-----  --  --  --  --  --  --   ------
 0     
 4
 8
12
16
20
24
28
32
36
```

#### C3: Did any ambiguities in the scheduler specification make values in the table uncertain?  If so, what rule did you use to resolve them?  Does this match the behavior of your scheduler?



#### C4: How is the way you divided the cost of scheduling between code inside and outside interrupt context likely to affect performance?



### Rationale

#### C5: Briefly critique your design, pointing out advantages and disadvantages in your design choices.  If you were to have extra time to work on this part of the project, how might you choose to refine or improve your design?



## Survey Questions

Answering these questions is optional, but it will help us improve the course in future quarters.  Feel free to tell us anything you want--these questions are just to spur your thoughts.  You may also choose to respond anonymously in the course evaluations at the end of the quarter.

In your opinion, was this assignment, or any one of the three problems in it, too easy or too hard?  Did it take too long or too little time?

Did you find that working on a particular part of the assignment gave you greater insight into some aspect of OS design?

Is there some particular fact or hint we should give students in future quarters to help them solve the problems?  Conversely, did you find any of our guidance to be misleading?

Do you have any suggestions for the TAs to more effectively assist students, either for future quarters or the remaining projects?

Any other comments?

