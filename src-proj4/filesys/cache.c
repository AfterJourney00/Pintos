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
struct cache_line*
check_hit_or_not(block_sector_t sec)
{
  struct cache_line* target_line = NULL;
  for(int i = 0; i < CACHE_SIZE; i ++){
    if(cache[i].valid_bit && sec == cache[i].sector_idx){
      target_line = &cache[i];
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
  cl->accessed_time = timer_ticks();
}

/* Clear a single cache line */
void
cache_line_clear(struct cache_line* cl)
{
  ASSERT(lock_held_by_current_thread(&cache_lock));

  cl->valid_bit = false;            /* Set this cache line as an invalid one */
  cl->dirty_bit = false;
}

/* Function for choosing a victim cache line to evict, using LRU policy */
struct cache_line*
next_cache_line_to_evict(void)
{
  ASSERT(lock_held_by_current_thread(&cache_lock)); 

  uint64_t earliest_time = LATEST_TIME;
  struct cache_line* target_line = NULL;

  for(int i = 0; i < CACHE_SIZE; i ++){
    if(!cache[i].valid_bit){
      continue;
    }
    else{
      if(cache[i].created_time < earliest_time){
        earliest_time = cache[i].created_time;
        target_line = &cache[i];
      }
    }
  }

  return target_line;
}