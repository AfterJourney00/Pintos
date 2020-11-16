#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stddef.h>
#include "lib/kernel/bitmap.h"

#define SIZE_PER_SECTOR 512
#define SECTORS_PER_PAGE 8

bool block_device_create(void);
size_t next_start_to_swap(void);
void write_into_swap_space(const void* dest);
void read_from_swap_space(size_t start_sector, void* dest);

#endif