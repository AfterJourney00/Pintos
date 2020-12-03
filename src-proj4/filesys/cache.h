#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

/* Data structure for one single cache line */
struct cache_line
{
  bool valid_bit;                   /* Only use cache line with true valid bit */
  bool dirty_bit;                   /* Only write back to disk with true dirty bit */
  int64_t accessed_time;            /* Use LRU cache eviction policy */

  block_sector_t sector_idx;       /* Record which sector should this cache line write back */
};

/* The whole cache, an array of cache lines */
struct cache_line cache[CACHE_SIZE];

/* Synchronization variable for cache system */
struct lock cache_lock;

/* Cache system operations */
void cache_init(void);
void cache_clear(void);
struct cache_line* check_hit_or_not(block_sector_t sec);

/* Cache line operations */
void cache_line_init(struct cache_line* cl);
void cache_line_clear(struct cache_line* cl);
struct cache_line* next_cache_line_to_evict(void);
void cache_write_back(void);
void cache_fetch_in(void);

#endif