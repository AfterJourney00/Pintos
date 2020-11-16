#include "vm/sup_page.h"
#include "vm/frame.h"
#include "threads/pte.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"

unsigned int
compute_page_hash_value(const struct hash_elem *e, void *aux UNUSED)
{
  struct supp_page* spage = hash_entry(e, struct supp_page, h_elem);
  return hash_bytes(&spage->user_vaddr, sizeof spage->user_vaddr);
}

bool
compare_page_hash_value(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  const struct supp_page* spage_a = hash_entry(a, struct supp_page, h_elem);
  const struct supp_page* spage_b = hash_entry(b, struct supp_page, h_elem);
  return spage_a->user_vaddr < spage_b->user_vaddr;
}

void
free_page_table_entry(const struct hash_elem *e, void *aux UNUSED)
{
  struct supp_page* spage = hash_entry(e, struct supp_page, h_elem);
  free(spage);
}

/* Given a hash table and a key, find the corresponding entry */
struct supp_page*
find_fake_pte(struct hash *hash_table, void *key)
{
  ASSERT(hash_table != NULL);     /* Assert the given hash table ptr is not a NULL */
  ASSERT(key != NULL);            /* Assert the given key ptr is not a NULL */

  struct supp_page* fake_pte = NULL;
  struct supp_page* target = malloc(sizeof(struct supp_page));
  if(target == NULL){
    goto done;
  }
  else{
    target->user_vaddr = key;
    struct hash_elem* he = hash_find(hash_table, &target->h_elem);
    
    free(target);
    if(he == NULL){
      goto done;
    }
    else{
      fake_pte = hash_entry(he, struct supp_page, h_elem);
    }
  }

done:
  return fake_pte;  
}

/* Create a supp_page entry(a reserved but not allocated page) */
bool
supp_page_entry_create(enum supp_type type, struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT(file != NULL);           /* Assert the given file ptr is not a NULL */
  ASSERT(is_user_vaddr(upage));   /* Assert the given user addr is really in user space */

  bool success = false;

  struct supp_page* spge = malloc(sizeof(struct supp_page));
  if(spge == NULL){
    goto done;
  }
  else{
    spge->type = type;

    /* Set all attributes of this new supplemental page entry */
    supp_page_entry_setting(spge, file, ofs, upage, read_bytes, zero_bytes, writable);

    /* Insert this new entry into current process's sup-page-table */
    struct hash_elem * he = hash_insert (&thread_current()->page_table, &spge->h_elem);

    if(he != NULL){     /* If this new sup-page-entry has existed */
      goto done;
    }

    success = true;
  }

done:
  return success;
}

/* Set all attributes of a given supp_page entry */
void supp_page_entry_setting(struct supp_page* sup, struct file *file, off_t ofs, uint8_t *upage,
                              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT(sup != NULL);            /* Assert the given sup_page_entry is not a NULL */

  sup->user_vaddr = upage;
  sup->file_in_this_page = file;
  sup->load_offset = ofs;
  sup->read_bytes = read_bytes;
  sup->zero_bytes = zero_bytes;
  sup->writable = writable;
  sup->fake_page = true;

  return;
}

bool
fake2real_page_convert(struct supp_page* spge)
{
  ASSERT(spge != NULL);
  // printf("fake2real: %p\n", spge->user_vaddr);
  uint8_t* upage = spge->user_vaddr; 
  struct file* loading_file = spge->file_in_this_page;
  off_t ofs = spge->load_offset;
  uint32_t read_bytes = spge->read_bytes;
  uint32_t zero_bytes = spge->zero_bytes;
  bool writable = spge->writable;

  ASSERT(upage != NULL);
  ASSERT(is_user_vaddr(upage));

  file_seek(loading_file, ofs);

  bool success = false;

  /* Get a page of memory(operations of 'load_segment in process.c') */
  struct frame* f = frame_create(PAL_USER);
  uint8_t *kpage = NULL;
  if(f == NULL){
    goto done;
  }
  else{
    kpage = f->frame_base;
  }
  
  if(file_read(loading_file, kpage, read_bytes) != (int)read_bytes){
    free_frame(f);
    goto done;
  }
  else{
    memset(kpage + read_bytes, 0, zero_bytes);
    if(!pagedir_set_page(thread_current()->pagedir, upage, kpage, writable)){
      free_frame(f);
      goto done;
    }
  }

  success = true;
  spge->fake_page = false;

done:
  return success;
}