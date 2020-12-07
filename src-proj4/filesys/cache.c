#include <debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "devices/timer.h"

#define LATEST_TIME 0xefffffff

/* Initialize the whole buffer cache */
void
cache_init(void)
{
  /* Initialize the cache lock */
  lock_init(&cache_lock);

  /* Initialize all 64 cache lines */
  lock_acquire(&cache_lock);
  for(int i = 0; i < CACHE_SIZE; i ++){
    cache_line_init(&cache[i]);
  }
  lock_release(&cache_lock);
  return;
}

/* Clear the whole buffer cache, should be used in filsys_done() */
void
cache_clear(void)
{
  /* Clear all 64 cache lines */
  lock_acquire(&cache_lock);
  for(int i = 0; i < CACHE_SIZE; i ++){
    cache_line_clear(&cache[i]);
  }
  lock_release(&cache_lock);
  return;
}

/* Function for checking cache hit or miss */
/* This function will be called before every access to cache,
   so update cache line's accessed_time here */
struct cache_line*
check_hit_or_not(block_sector_t sec)
{
  ASSERT(lock_held_by_current_thread(&cache_lock));

  struct cache_line* target_line = NULL;
  for(int i = 0; i < CACHE_SIZE; i ++){
    if(cache[i].valid_bit && sec == cache[i].sector_idx && !cache[i].available){
      target_line = &cache[i];                  /* Find the candidate line */
      cache[i].accessed_time = timer_ticks();   /* Update its accessed time */
      goto done;
    }
  }

done:
  return target_line;
}

/* Initialize a single cache line */
void
cache_line_init(struct cache_line* cl)
{
  ASSERT(lock_held_by_current_thread(&cache_lock));

  cl->valid_bit = true;
  cl->dirty_bit = false;
  cl->available = true;
  cl->accessed_time = timer_ticks();
  cl->buffer = (char*)malloc(BLOCK_SECTOR_SIZE);
  ASSERT(cl->buffer != NULL);

  return;
}

/* Clear a single cache line (don't know yet)*/
void
cache_line_clear(struct cache_line* cl)
{
  ASSERT(lock_held_by_current_thread(&cache_lock));

  if(cl->dirty_bit){
    cache_write_back(cl);
  }
  cl->valid_bit = false;
  return;
}

/* Find a free cache line if cache miss */
struct cache_line*
fetch_a_free_cache_line(void)
{
  ASSERT(lock_held_by_current_thread(&cache_lock));

  struct cache_line* target_line = NULL;
  for(int i = 0; i < CACHE_SIZE; i ++){
    if(cache[i].valid_bit && cache[i].available){
      target_line = &cache[i];
      goto done;
    }
  }

done:
  return target_line;
}

/* Function for choosing a victim cache line to evict, using LRU policy */
struct cache_line*
next_cache_line_to_evict(void)
{
  ASSERT(lock_held_by_current_thread(&cache_lock)); 

  uint64_t earliest_time = LATEST_TIME;
  struct cache_line* target_line = NULL;

  for(int i = 0; i < CACHE_SIZE; i ++){
    ASSERT(cache[i].available == false);

    if(!cache[i].valid_bit){    /* If the cache line is invalid, pass it whatever */
      continue;
    }
    else{                       /* Else, choose a victim line using LRU policy */
      if(cache[i].accessed_time < earliest_time){
        earliest_time = cache[i].accessed_time;
        target_line = &cache[i];
      }
    }
  }

  return target_line;
}

/* Function for cache line eviction */
void
evict_cache_line(struct cache_line* cl)
{
  ASSERT(cl != NULL);
  ASSERT(cl->valid_bit == true && cl->available == false);
  
  if(cl->dirty_bit){
    cache_write_back(cl);
  }
  /* cl->available = true; */
  cl->dirty_bit = false;
  return;
}

/* Write back from cache to disk */
void
cache_write_back(struct cache_line* cl)
{
  /* Assert the given cache line is a valid one */
  ASSERT(cl != NULL);
  ASSERT(cl->valid_bit == true && cl->dirty_bit == true);

  block_write(fs_device, cl->sector_idx, cl->buffer);
  return;
}

/* Fetch in from disk to cache */
void
cache_fetch_in(struct cache_line* cl)
{
  /* Assert the given cache line is a valid one */
  ASSERT(cl != NULL);
  ASSERT(cl->valid_bit == true);

  block_read(fs_device, cl->sector_idx, cl->buffer);
}

/* Other parts through this function to access cache and do operations */
/* read_or_write = true: read 
   read_or_write = false: write */
void
cache_do(bool read_or_write, block_sector_t sec, void* mem_addr)
{
  lock_acquire(&cache_lock);

  struct cache_line* target_line = check_hit_or_not(sec);
  if(target_line == NULL){/* Cache miss */
    target_line = fetch_a_free_cache_line();
    if(target_line == NULL){          /* Need to do eviction */
      target_line = next_cache_line_to_evict();
      evict_cache_line(target_line);
    }

    ASSERT(target_line->valid_bit == true);

    target_line->available = false;             /* Set this cache line as a busy line */
    target_line->accessed_time = timer_ticks(); /* Update its accessed time */
    target_line->sector_idx = sec;              /* Record the sector index */
    target_line->dirty_bit = !read_or_write;
    cache_fetch_in(target_line);
  }

  ASSERT(target_line != NULL);

  if(read_or_write){
    memcpy(mem_addr, (const void*)(target_line->buffer), BLOCK_SECTOR_SIZE);
  }
  else{
    target_line->dirty_bit = true;
    memcpy((void*)(target_line->buffer), (const void*)mem_addr, BLOCK_SECTOR_SIZE);
  }

  lock_release(&cache_lock);
  return;
}