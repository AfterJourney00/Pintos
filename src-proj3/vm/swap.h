#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stddef.h>
#include "lib/kernel/bitmap.h"

#define SIZE_PER_SECTOR 512
#define SECTORS_PER_PAGE 8

bool block_device_create(void);
size_t next_start_to_swap(void);
size_t write_into_swap_space(const void* dest);
size_t read_from_swap_space(size_t start_sector, void* dest);
void free_swap_slot(size_t swap_idx);

#endif