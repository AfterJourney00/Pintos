#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  cache_init();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  cache_clear();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  ASSERT(name != NULL);

  block_sector_t inode_sector = 0;

  /* Decomposite directory name and file name */
  size_t path_length = strlen (name) + 1;
  char* dir_name = (char*)malloc(path_length);
  char* file_name = (char*)malloc(path_length);
  split_path_to_dir_filename(name, dir_name, file_name);

  struct dir *dir;
  dir = dir_general_open(dir_name);

  /* Open the directory */
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector, is_dir));
  if (!success && inode_sector != 0){
    free_map_release (inode_sector, 1);
  }
  dir_close (dir);

  free(dir_name);
  free(file_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  ASSERT(name != NULL);

  size_t path_length = strlen (name) + 1;
  char* dir_name = (char*)malloc(path_length);
  char* file_name = (char*)malloc(path_length);
  split_path_to_dir_filename(name, dir_name, file_name);
  if(strlen(dir_name) == 0 && strlen(file_name) == 0){    /* Empty */
    return NULL;
  }

  struct dir *dir;
  dir = dir_general_open(dir_name);
  struct inode *inode = NULL;

  if(dir == NULL){
    return NULL;
  }
  
  if(strlen(file_name) == 0 || strcmp(file_name, ".") == 0){
    inode = dir_get_inode(dir);
  }
  else{
    dir_lookup (dir, file_name, &inode);
  }
  dir_close(dir);
  if(inode == NULL || inode_is_removed(inode)){
    return NULL;
  }
  free(dir_name);
  free(file_name);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  ASSERT(name != NULL);

  // struct dir *dir = dir_open_root ();
  size_t path_length = strlen (name) + 1;
  char* dir_name = (char*)malloc(path_length);
  char* file_name = (char*)malloc(path_length);
  split_path_to_dir_filename(name, dir_name, file_name);

  struct dir *dir;
  dir = dir_general_open(dir_name);

  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 
  
  free(dir_name);
  free(file_name);
  return success;
}

/* Change the process's current working directory, 
   interface provide for syscall
*/
bool
filesys_change_dir(const char *dir)
{
  bool success = false;
  struct thread *cur = thread_current();
  struct dir* newdir = dir_general_open(dir);
  if(newdir == NULL){
    goto done;
  }
  dir_close(cur->cwd);
  cur->cwd = newdir;
  success = true;

done:
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
