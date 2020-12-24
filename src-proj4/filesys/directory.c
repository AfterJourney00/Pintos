#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  bool success = inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
  if(!success){
    return success;
  }
  struct dir_entry e;
  size_t entry_size = sizeof(struct dir_entry);
  struct inode* cur_inode = inode_open(sector);
  ASSERT(cur_inode != NULL);

  struct dir* cur_dir = dir_open(cur_inode);
  ASSERT(cur_dir != NULL);

  e.inode_sector = sector;
  off_t written_bytes = inode_write_at(cur_dir->inode, &e, entry_size, 0);
  dir_close(cur_dir);
  return written_bytes == entry_size;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = sizeof(struct dir_entry);
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* A general function for directory opening: 
   Take a full path as input and open the directory layer by layer
*/
struct dir *
dir_general_open(const char* full_path)
{
  ASSERT(full_path != NULL);

  struct thread* cur = thread_current();
  size_t path_length = strlen(full_path) + 1;
  char* full_path_copy = (char*)malloc(path_length);
  memcpy(full_path_copy, full_path, path_length);

  struct dir* parent;
  if(full_path_copy[0] == '/' || cur->cwd == NULL){
    parent = dir_open_root();
  }
  else{
    parent = dir_reopen(cur->cwd);
  }
  printf("marching here\n");
  char* save_ptr;
  for(char* s = strtok_r(full_path_copy, "/", &save_ptr);
            s != NULL;
            s = strtok_r(NULL, "/", &save_ptr)){
    struct inode* sub;
    if(!dir_lookup(parent, s, &sub)){     /* No sub dir in parent dir */
      printf("is here?\n");
      dir_close(parent);
      goto done;
    }

    struct dir* sub_dir = dir_open(sub);
    if(sub_dir == NULL){/* Sub dir open failed */
      dir_close(parent);
      goto done;
    }

    dir_close(parent);
    if(inode_is_removed(sub)){
      goto done;
    }
    parent = sub_dir;
  }
  free(full_path_copy);
  return parent;

done:
  free(full_path_copy);
  return NULL;
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = sizeof e; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;
  size_t entry_size = sizeof(struct dir_entry);

  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  
  if(strcmp(name, ".") == 0){        /* We are looking up current directory */
    printf("case1\n");
    *inode = inode_reopen(dir->inode);
  }
  else if(strcmp(name, "..") == 0){  /* We are looking up the parent directory */
    printf("case2\n");
    if(inode_read_at(dir->inode, &e, entry_size, 0) != entry_size){
      return NULL;
    }
    *inode = inode_open(e.inode_sector);
  }
  else if (lookup (dir, name, &e, NULL)){
    printf("case3\n");
    *inode = inode_open (e.inode_sector);
  }
  else{
    printf("name: %s\n", name);
    printf("case4\n");
    *inode = NULL;
  }
  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, bool is_dir)
{
  struct dir_entry e;
  size_t entry_size = sizeof(struct dir_entry);
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0'  || strlen (name) > NAME_MAX){
    return false;
  }
  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Record the parent directory(dir) in the subdir inode[first dir_entry] */
  if(is_dir){
    struct inode* subdir_inode = inode_open(inode_sector);
    if(subdir_inode == NULL){
      return false;
    }

    struct dir* subdir = dir_open(subdir_inode);
    if(subdir == NULL){
      return false;
    }
    inode_read_at(dir->inode, &e, entry_size, 0);
    inode_write_at(subdir->inode, &e, entry_size, 0);
    dir_close(subdir);
  }

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = sizeof e; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* 
    Check if the file we want to remove is a directory.
    If so, return false if this directory is not empty.
  */
  if(inode_is_dir(inode)){
    struct dir* subdir = dir_open(inode);
    struct dir_entry de;
    size_t entry_size = sizeof(struct dir_entry);
    for(off_t offset = entry_size;
        inode_read_at(subdir->inode, &de, entry_size, offset) == entry_size;
        offset += entry_size){
      if(de.in_use){
        dir_close(subdir);
        goto done;
      }
    }
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;
  
  /* Remove inode. */
  inode_remove (inode);
  success = true;
  
 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  // printf("pos: %d\n", dir->pos);
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      // printf("begin loop\n");
      dir->pos += sizeof e;
      // printf("pos: %d\n", dir->pos);
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
      // printf("finish loop\n");
    }
  return false;
}

void
split_path_to_dir_filename(const char* full_path, char* dir, char* filename)
{
  ASSERT(full_path != NULL && dir != NULL && filename != NULL);

  /* Create a backup of the full path */
  size_t path_length = strlen(full_path) + 1;
  char* full_path_copy = (char*)malloc(path_length);
  memcpy(full_path_copy, full_path, path_length);

  /* Coordinate the location of filename(last token) */
  size_t i = path_length - 2;
  for(i = path_length - 2; (int)i >= 0; i --){
    if(full_path_copy[i] == '/'){
      break;
    }
  }
  
  /* Split the directory and file name */
  memcpy(filename, full_path_copy + i+1, path_length - i-1);
  full_path_copy[i+1] = '\0';
  memcpy(dir, full_path_copy, i + 2);

  free(full_path_copy);
  return;
}