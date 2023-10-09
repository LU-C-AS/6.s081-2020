// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "types.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem, kmem_cpu[NCPU];

void kinit() {
  initlock(&kmem.lock, "kmem");
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem_cpu[i].lock, "kmem_");
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  push_off();
  int i = cpuid();
  pop_off();

  acquire(&kmem_cpu[i].lock);
  r->next = kmem_cpu[i].freelist;
  kmem_cpu[i].freelist = r;
  release(&kmem_cpu[i].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;

  push_off();
  int i = cpuid();
  pop_off();

  acquire(&kmem_cpu[i].lock);
  r = kmem_cpu[i].freelist;
  if (r) {
    kmem_cpu[i].freelist = r->next;
    release(&kmem_cpu[i].lock);
  } else {
    release(&kmem_cpu[i].lock);
    for (int j = 0; j < NCPU; j++) {
      if (j == i)
        continue;
      acquire(&kmem_cpu[j].lock);
      if (kmem_cpu[j].freelist) {
        r = kmem_cpu[j].freelist;
        kmem_cpu[j].freelist = r->next;
        release(&kmem_cpu[j].lock);
        break;
      }
      release(&kmem_cpu[j].lock);
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
