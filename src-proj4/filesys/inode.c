#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Multi level sectors */
#define FIRST_LAYER_SECTORS 123
#define SECTORS_PER_SECTOR 128
#define SECOND_LAYER_SECTORS SECTORS_PER_SECTOR

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct_sectors[FIRST_LAYER_SECTORS];
    block_sector_t indirect_sector_idx;
    block_sector_t doubly_indirect_sector_idx;

    bool is_dir;                        /* Record this file is a directory or not */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

/* Definition of Indexed and extensible file inodes allocation functions */
bool freemap_single_sector_create(block_sector_t* sec);
bool direct_inode_create(struct inode_disk *disk_inode, size_t* sectors, off_t ofs);
bool indirect_inode_create1(struct inode_disk *disk_inode, size_t* sectors, off_t ofs);
bool indirect_inode_create2(struct inode_disk *disk_inode, size_t* sectors, off_t ofs);
bool indexed_inode_allocate(struct inode_disk *disk_inode, size_t sectors);

/* Definition of Indexed and extensible file inodes deallocation functions */
bool direct_inode_dealloc(struct inode_disk *disk_inode, size_t* sectors);
bool indirect_inode_dealloc1(struct inode_disk *disk_inode, size_t* sectors);
bool indirect_inode_dealloc2(struct inode_disk *disk_inode, size_t* sectors);
bool indexed_inode_dealloc(struct inode_disk *disk_inode, size_t sectors);

/* Helper function */
void zero_array_init(block_sector_t* array);


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT(inode != NULL);
  if(pos < 0 || pos >= inode->data.length){
    return -1;
  }

  block_sector_t indirect_sectors_array1[SECOND_LAYER_SECTORS];
  block_sector_t indirect_sectors_array2[SECOND_LAYER_SECTORS];
  zero_array_init(indirect_sectors_array1);
  zero_array_init(indirect_sectors_array2);
  off_t idx = pos / BLOCK_SECTOR_SIZE;
  if(idx < FIRST_LAYER_SECTORS){           /* Direct sectors can cover */
    return inode->data.direct_sectors[idx];
  }
  else{                                    /* Indirect sectors can cover */
    idx -= FIRST_LAYER_SECTORS;
    if(idx < SECOND_LAYER_SECTORS){
      /* Read the indirect sector table in */
      cache_do(true, inode->data.indirect_sector_idx, indirect_sectors_array1);
      return indirect_sectors_array1[idx];
    }
    else{                                           /* Doubly indirect sectors can cover */
      idx -= SECOND_LAYER_SECTORS;
      ASSERT(idx < SECTORS_PER_SECTOR * SECTORS_PER_SECTOR);
      size_t double_indirect_idx1 = idx / SECTORS_PER_SECTOR;
      size_t double_indirect_idx2 = idx % SECTORS_PER_SECTOR;
      cache_do(true, inode->data.doubly_indirect_sector_idx, indirect_sectors_array1);
      cache_do(true, indirect_sectors_array1[double_indirect_idx1], indirect_sectors_array2);
      return indirect_sectors_array2[double_indirect_idx2];
    }
  }
  return -1;     /* Error case */
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);
  
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->is_dir = is_dir;
      disk_inode->magic = INODE_MAGIC;
      if(indexed_inode_allocate(disk_inode, sectors)){
        cache_do(false, sector, disk_inode);
        success = true;
      }
      free (disk_inode);
    }

  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  // block_read (fs_device, inode->sector, &inode->data);
  cache_do(true, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          indexed_inode_dealloc(&inode->data, bytes_to_sectors(inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  ASSERT(inode != NULL && buffer_ != NULL);

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          // block_read (fs_device, sector_idx, buffer + bytes_read);
          cache_do(true, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          // block_read (fs_device, sector_idx, bounce);
          cache_do(true, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt){
    return 0;
  }

  /* Before write, check whether need to do file extension and do it if needed */
  if(byte_to_sector(inode, offset + size - 1) == -1u){
    /* Calculate:
          1. How many sectors have been allocated and occupied
          2. How many bytes out of size
          3. How many sectors should be extended
    */
    size_t occupied_sectors = bytes_to_sectors(inode->data.length);
    off_t lack_bytes = offset + size - inode->data.length;
    size_t lack_sectors = bytes_to_sectors(lack_bytes);

    /* Record: 
          1. Which level we should start to extend: Direct(0), Indirect(1), Doubly_indirect(2)
          2. The offset we should start at: e.g. (Direct, 10) or (Indirect, 20)
    */
    int start_layer = 0;        
    if(occupied_sectors >= FIRST_LAYER_SECTORS){
      start_layer = 1;
      occupied_sectors -= FIRST_LAYER_SECTORS;
      if(occupied_sectors >= SECOND_LAYER_SECTORS){
        start_layer = 2;
        occupied_sectors -= SECOND_LAYER_SECTORS;
        ASSERT(occupied_sectors < SECTORS_PER_SECTOR * SECTORS_PER_SECTOR);
      }
    }
    /* Do extension, return 0 if any failure occurs */
    if(start_layer == 0){
      if(!direct_inode_create(&inode->data, &lack_sectors, occupied_sectors)){
        return 0;
      }
      if(lack_bytes > 0){
        if(!indirect_inode_create1(&inode->data, &lack_sectors, 0)){
          return 0;
        }
      }
      if(lack_sectors > 0){
        if(!indirect_inode_create2(&inode->data, &lack_sectors, 0)){
          return 0;
        }
      }
    }
    else if(start_layer == 1){
      if(!indirect_inode_create1(&inode->data, &lack_sectors, occupied_sectors)){
        return 0;
      }
      if(lack_sectors > 0){
        if(!indirect_inode_create2(&inode->data, &lack_sectors, 0)){
          return 0;
        }
      }
    }
    else{
      if(!indirect_inode_create2(&inode->data, &lack_sectors, occupied_sectors)){
        return 0;
      }
    }

    /* Assert that we extend all we need to extend */
    ASSERT(lack_sectors == 0);  

    /* Update the metadata of the file */
    inode->data.length = offset + size;
    cache_do(false, inode->sector, &inode->data);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          // block_write (fs_device, sector_idx, buffer + bytes_written);
          cache_do(false, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left){
            // block_read (fs_device, sector_idx, bounce);
            cache_do(true, sector_idx, bounce);
          }
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // block_write (fs_device, sector_idx, bounce);
          cache_do(false, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

// Functions for proj4

/* Helper function */
void
zero_array_init(block_sector_t* array)
{
  for(int i = 0; i < SECTORS_PER_SECTOR / 4; i += 4){
    array[i] = 0;
    array[i+1] = 0;
    array[i+2] = 0;
    array[i+3] = 0;
  }
  return;
}

/* Function for allocating a single sector using freemap */
bool
freemap_single_sector_create(block_sector_t* sec)
{
  ASSERT(sec != NULL);

  bool success = false;
  static char zeros[BLOCK_SECTOR_SIZE];   /* Useless data for intialization */

  if(!free_map_allocate(1, sec)){          /* Create a sector of space using freemap */
    goto done;
  }
  cache_do(false, *sec, zeros);           /* Write the initialization data into the sector */
  success = true;

done:
  return success;
}

/* Direct inode allocate function */
bool
direct_inode_create(struct inode_disk *disk_inode, size_t* sectors, off_t ofs)
{
  ASSERT(disk_inode != NULL);

  bool success = false;
  for(int i = ofs; i < FIRST_LAYER_SECTORS && *sectors > 0; i ++){
    if(!freemap_single_sector_create(&disk_inode->direct_sectors[i])){
      goto done;
    }
    *sectors -= 1;
  }
  success = true;

done:
  return success;
}

bool
indirect_inode_create1(struct inode_disk *disk_inode, size_t* sectors, off_t ofs)
{
  bool success = false;
  block_sector_t indirect_sectors_array[SECTORS_PER_SECTOR];

  /* Initialize the array to a zero array */
  zero_array_init(indirect_sectors_array);

  if(disk_inode->indirect_sector_idx != 0){
    cache_do(true, disk_inode->indirect_sector_idx, indirect_sectors_array);
  }
  else{
    if(!freemap_single_sector_create(&disk_inode->indirect_sector_idx)){
      goto done;
    }
  }

  for(int i = ofs; i < SECOND_LAYER_SECTORS && *sectors > 0; i++){
    if(!freemap_single_sector_create(&indirect_sectors_array[i])){
      goto done;
    }
    *sectors -= 1;
  }
  cache_do(false, disk_inode->indirect_sector_idx, indirect_sectors_array);
  success = true;

done:
  return success;
}

bool
indirect_inode_create2(struct inode_disk *disk_inode, size_t* sectors, off_t ofs)
{
  block_sector_t indirect_sectors_array1[SECTORS_PER_SECTOR];
  block_sector_t indirect_sectors_array2[SECTORS_PER_SECTOR];
  zero_array_init(indirect_sectors_array1);
  zero_array_init(indirect_sectors_array2);
  bool success = false;
  
  /* Check the doubly indirect sector array is allocated or not */
  if(disk_inode->doubly_indirect_sector_idx != 0){
    /* If allocated already, read it from cache */
    cache_do(true, disk_inode->doubly_indirect_sector_idx, indirect_sectors_array1);
  }
  else{
    /* If not allocated yet, allocate one using freemap */
    if(!freemap_single_sector_create(&disk_inode->doubly_indirect_sector_idx)){
      goto done;
    }
  }
  
  /* Start from the OFS-th sector, 
       suppose ofs = 10, this means we start from 0->10 to extend
       suppose ofs = 129, this means we start from 1->1 */
  int offset1 = ofs / SECTORS_PER_SECTOR;
  int offset2 = ofs % SECTORS_PER_SECTOR;
  for(int i = offset1; i < SECOND_LAYER_SECTORS && *sectors > 0; i++){
    /* If allocate at this level already, read it from cache */
    if(indirect_sectors_array1[i] == 0){
      if(!freemap_single_sector_create(&indirect_sectors_array1[i])){
        goto done;
      }
    }
    else{
      cache_do(true, indirect_sectors_array1[i], indirect_sectors_array2);
    }
    
    for(int j = offset2; j < SECOND_LAYER_SECTORS && *sectors > 0; j++){
      if(!freemap_single_sector_create(&indirect_sectors_array2[j])){
        goto done;
      }
      *sectors -= 1;
    }
    cache_do(false, indirect_sectors_array1[i], indirect_sectors_array2);
    offset2 = 0;
  }
  cache_do(false, disk_inode->doubly_indirect_sector_idx, indirect_sectors_array1);
  success = true;

done:
  return success;
}

bool
indexed_inode_allocate(struct inode_disk *disk_inode, size_t sectors)
{
  ASSERT(disk_inode != NULL);
  
  bool success = false;
  // static char zeros[BLOCK_SECTOR_SIZE];     /* Useless data for initialization */

  /* Pre check whether indirect sectors are needed */
  bool is_indirect_needed = (int)sectors > (int)FIRST_LAYER_SECTORS;
  bool is_doubly_indirect_needed = (int)(sectors - FIRST_LAYER_SECTORS) > (int)(SECOND_LAYER_SECTORS);

  /* Fill direct sectors */
  if(!direct_inode_create(disk_inode, &sectors, 0)){
    goto done;
  }

  /* Fill indirect sectors */
  if(is_indirect_needed){
    if(!indirect_inode_create1(disk_inode, &sectors, 0)){
      goto done;
    }
  }

  /* Fill doubly indirect sectors */
  if(is_doubly_indirect_needed){
    if(!indirect_inode_create2(disk_inode, &sectors, 0)){
      goto done;
    }
  }

  ASSERT(sectors == 0);
  success = true;

done:
  return success;
}

/* Deallocate direct inodes */
bool
direct_inode_dealloc(struct inode_disk *disk_inode, size_t* sectors)
{
  ASSERT(disk_inode != NULL);

  for(int i = 0; i < FIRST_LAYER_SECTORS && *sectors > 0; i ++){
    free_map_release(disk_inode->direct_sectors[i], 1);
    *sectors -= 1;
  }

  return true;
}

/* Deallocate indirect inodes */
bool
indirect_inode_dealloc1(struct inode_disk *disk_inode, size_t* sectors)
{
  ASSERT(disk_inode != NULL);
  ASSERT(*sectors > 0);

  block_sector_t indirect_sectors_array[SECOND_LAYER_SECTORS];
  cache_do(true, disk_inode->indirect_sector_idx, indirect_sectors_array);

  for(int i = 0; i < SECOND_LAYER_SECTORS && *sectors > 0; i++){
    free_map_release(indirect_sectors_array[i], 1);
    *sectors -= 1;
  }
  
  return true;
}

/* Deallocate doubly_indirect inodes */
bool
indirect_inode_dealloc2(struct inode_disk *disk_inode, size_t* sectors)
{
  ASSERT(disk_inode != NULL);
  ASSERT(*sectors > 0);

  block_sector_t indirect_sectors_array1[SECOND_LAYER_SECTORS];
  block_sector_t indirect_sectors_array2[SECOND_LAYER_SECTORS];
  cache_do(true, disk_inode->doubly_indirect_sector_idx, indirect_sectors_array1);

  for(int i = 0; i < SECOND_LAYER_SECTORS && *sectors > 0; i++){
    cache_do(true, indirect_sectors_array1[i], indirect_sectors_array2);
    for(int j = 0; j < SECOND_LAYER_SECTORS && *sectors > 0; j++){
      free_map_release(indirect_sectors_array2[j], 1);
      *sectors -= 1;
    }
    free_map_release(indirect_sectors_array1[i], 1);
  }

  return true;
}

/* Function for deallocate indexed and extensible file inodes */
bool
indexed_inode_dealloc(struct inode_disk *disk_inode, size_t sectors)
{
  ASSERT(disk_inode != NULL);

  bool success = false;

  /* Pre check whether indirect sectors are needed */
  bool is_indirect_needed = (int)sectors > (int)FIRST_LAYER_SECTORS;
  bool is_doubly_indirect_needed = (int)(sectors - FIRST_LAYER_SECTORS) > (int)SECOND_LAYER_SECTORS;

  /* Deallocate direct sectors */
  direct_inode_dealloc(disk_inode, &sectors);

  /* Fill indirect sectors */
  if(is_indirect_needed){
    indirect_inode_dealloc1(disk_inode, &sectors);
  }

  /* Fill doubly indirect sectors */
  if(is_doubly_indirect_needed){
    indirect_inode_dealloc2(disk_inode, &sectors);
  }

  ASSERT(sectors == 0);
  success = true;

done:
  return success;
}

bool
inode_is_removed(struct inode * node)
{
  ASSERT(node != NULL);
  return node->removed;
}

bool
inode_is_dir(struct inode * node)
{
  ASSERT(node != NULL);
  return node->data.is_dir;
}