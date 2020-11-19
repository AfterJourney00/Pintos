#ifndef VM_SUP_PAGE_H
#define VM_SUP_PAGE_H

#include <stdio.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"

/* Three types of supplemental pte. One pte can only has one type */
enum supp_type
{
  LAZY_LOAD,          /* Only supplemental page table entry exists, for lazy load */
  CO_EXIST,           /* Both supplemental pte and real page exist */
  EVICTED,            /* Only supplemental page table entry exists, for swap */
};

/* Supplementary page table(assistor of real page table)
   We record some information here to implement lazy loading and eviction */
struct supp_page
{
  enum supp_type type;            /* Indicate the type of this supplemental pte */

  uint8_t* user_vaddr;            /* The user virtual address corresponding to this page entry*/
  struct file* file_in_this_page; /* Record the owner of this page */
  off_t load_offset;              /* Record the loading start */
  uint32_t read_bytes;            /* Record how many bytes to read */
  uint32_t zero_bytes;            /* Record how many bytes to set to 0 */
  bool writable;                  /* Record the writable attribute of this page */

  bool fake_page;                 /* Inidicate this page is a fake page or real */ 

  size_t swap_idx;                /* Record which swap slot evicted to(only for type:EVICTED) */  
  
  struct hash_elem h_elem;    /* Element for hash table */
};

/* Auxilary functionality for hash page table */
unsigned int compute_page_hash_value(const struct hash_elem *e, void *aux UNUSED);
bool compare_page_hash_value(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void free_page_table_entry(const struct hash_elem *e, void *aux UNUSED);

/* Basic lifecycle of a sup_page entry */
bool supp_page_entry_create(enum supp_type type, struct file *file, off_t ofs, uint8_t *upage,
                            uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void entry_setting_lazy(struct supp_page* sup, struct file *file, off_t ofs, uint8_t *upage,
                              uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void entry_setting_co(struct supp_page* sup, uint8_t *upage);

/* Auxilary functionality for other parts */
struct supp_page* find_fake_pte(struct hash *hash_table, void *key);
bool fake2real_page_convert(struct supp_page* spge);
bool create_evicted_pte(struct thread* t, size_t swap_idx, void* uvaddr);
bool real2evicted_page_convert(struct supp_page* spge, size_t swap_idx);
bool try_to_do_reclaimation(struct supp_page* spge);
bool try_to_unmap(struct supp_page* spge, int advance, int write_length);

#endif