#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <stdio.h>

struct block* block_device;
struct bitmap* swap_space_map;
struct lock swap_lock;

bool
block_device_create(void)
{
  bool success = false;

  block_device = block_get_role(BLOCK_SWAP);
  if(block_device == NULL){
    goto done;
  } 

  swap_space_map = bitmap_create(block_size(block_device) / PGSIZE);
  if(swap_space_map){
    goto done;
  }

  lock_init(&swap_lock);

  success = true;

done:
  return success;
}

size_t
next_start_to_swap(void)
{
  lock_acquire(&swap_lock);
  size_t next_start = bitmap_scan_and_flip(swap_space_map, 0, 1, false);
  lock_release(&swap_lock);

  return next_start;
}

void
write_into_swap_space(const void* dest)
{
  ASSERT(is_user_vaddr(dest));

  /* Choose a start of a consecutive page-sized space to write */
  size_t next_start = next_start_to_swap();
  ASSERT(next_start != BITMAP_ERROR);

  /* Write to this page-sized space 8 times(512 bytes per time) */
  lock_acquire(&swap_lock);
  for(int i = 0; i < SECTORS_PER_PAGE; i++){
    block_write(block_device, next_start * SECTORS_PER_PAGE + i, dest + i * SIZE_PER_SECTOR);
  }
  lock_release(&swap_lock);

  return;
}

void
read_from_swap_space(size_t start_sector, void* dest)
{
  ASSERT(is_user_vaddr(dest));

  /* Read from this page-sized space 8 times(512 bytes per time) */
  lock_acquire(&swap_lock);
  for(int i = 0; i < SECTORS_PER_PAGE; i++){
    block_read(block_device, start_sector * SECTORS_PER_PAGE + i, dest + i * SIZE_PER_SECTOR);
  }

  /* Mark the corresponding page-sized region is available */
  bitmap_flip(swap_space_map, start_sector);
  lock_release(&swap_lock);

  return;
}