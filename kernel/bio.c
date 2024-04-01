// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "buf.h"
#include "defs.h"
#include "fs.h"
#include "param.h"
#include "riscv.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "types.h"

#define NBUC 13
extern uint ticks;
// #define NBUFF NBUF + 1
struct {
  struct spinlock lock;
  struct spinlock mlock;
  struct buf buf[NBUF];

  uint time[NBUF];
  // uint ind;
  uint32 bmap; // 用位是否为1指示对应的cache block是否被使用
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

// struct bucket {
//   struct buf *b;
//   // struct bucket *next;
//   struct buf head;
// } buck[NBUC];
//
// buck[i] is a head node
struct buf buck[NBUC];
struct spinlock bulock[NBUC];

uint lru_i(void) {
  uint t = bcache.time[0];
  int ind = 0;
  for (int i = 0; i < NBUF; i++) {
    if (bcache.time[i] < t) {
      t = bcache.time[i];
      ind = i;
    }
  }
  return ind;
}

void binit(void) {
  // struct buf *b;

  initlock(&bcache.lock, "bcache");
  initlock(&bcache.mlock, "bcache_m");
  // bcache.ind = -1;
  bcache.bmap = 0x3FFFFFFF;
  for (int i = 0; i < NBUC; i++) {
    initlock(&bulock[i], "bcache_buck");
    buck[i].next = 0;
    buck[i].prev = 0;
  }
  for (int i = 0; i < NBUF; i++) {
    bcache.buf[i].next = 0;
    bcache.buf[i].prev = 0;
    bcache.time[i] = 0xFFFFFFFF;
    // if (buck[NBUC].next) {
    //   buck[NBUC].next->prev = bcache.buf + i;
    // }
    // bcache.buf[i].next = buck[NBUC].next;
    // bcache.buf[i].prev = buck + NBUC;
    // buck[NBUC].next = bcache.buf + i;
    initsleeplock(&bcache.buf[i].lock, "buffer");
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;

  // acquire(&bcache.lock);

  int i = blockno % NBUC;
  // printf("bget bulock %d\n", i);

  acquire(&bulock[i]);

  // Is the block already cached?
  for (b = buck[i].next; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      // printf("bget bu unlock cached %d\n", i);

      acquire(&bcache.lock);
      // bcache.ind = b - bcache.buf;
      acquire(&tickslock);
      bcache.time[b - bcache.buf] = ticks;
      release(&tickslock);
      release(&bcache.lock);
      release(&bulock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // for (b = bcache.head.next; b != &bcache.head; b = b->next) {
  //   if (b->dev == dev && b->blockno == blockno) {
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  acquire(&bcache.mlock);
  int n = 0;
  // printf("%x %x\n", bcache.bmap, 0x1 << 1);
  while (((bcache.bmap >> n) & 0x1) != 1 && n != NBUF) {
    // printf("%d %x %x\n", n, 0x1 << n, bcache.bmap);
    n++;
  }
  if (n < NBUF) {
    bcache.bmap &= (~(0x1 << n));
    b = bcache.buf + n;
  }
  // printf("%d %d\n", 0x1 << n, (bcache.bmap & (0x1 << n)));
  release(&bcache.mlock);
  // printf("%d %x\n", n, bcache.bmap);
  if (n < NBUF)
    goto found;
  // acquire(&bulock[NBUC]);
  // if ((b = buck[NBUC].next) != 0) {
  //   if (b->next) {
  //     b->next->prev = b->prev;
  //   }
  //   b->prev->next = b->next;
  // }
  // release(&bulock[NBUC]);
  // goto found;

  // printf("%d\n", bcache.ind);
  acquire(&bcache.lock);
  int ind = lru_i();
  // printf("j %d\n", ind);
  int no = bcache.buf[ind].blockno;
  int j = no % NBUC;
  if (i != j) {
    acquire(&bulock[j]);
  }
  b = bcache.buf + j;

  if (b->next) {
    b->next->prev = b->prev;
  }
  b->prev->next = b->next;
  if (i != j) {
    release(&bulock[j]);
  }
  goto found;
  // int j;
  // for (j = 0; j < NBUF; j++) {
  //   acquire(&bcache.lock);
  //   if (bcache.buf[j].refcnt == 0 || bcache.time[j] == 0) { // free block
  //     b = bcache.buf + j;
  //     acquire(&tickslock);
  //     bcache.time[j] = ticks;
  //     release(&tickslock);
  //     goto found;
  //   } else if (bcache.time[j] < mint) {
  //     mint = bcache.time[j];
  //     b = bcache.buf + j;
  //   }
  //   release(&bcache.lock);
  // }

  // int ind = b->blockno % NBUC;
  // if (ind != i) {
  //   // release(&bcache.lock);
  //   acquire(&bulock[ind]);
  // }
  //
  // acquire(&tickslock);
  // bcache.time[ind] = ticks;
  // release(&tickslock);
  // // struct buf *p;
  // // for (p = buck[ind].next; p != b && p != 0; p = p->next)
  // // ;
  // if (b->next) {
  //   b->next->prev = b->prev;
  // }
  // b->prev->next = b->next;
  // if (ind != i) {
  //   release(&bulock[ind]);
  //   // acquire(&bcache.lock);
  // }
  // acquire(&bcache.lock);
  // goto found;

found:
  // printf("found %d\n", b - bcache.buf);
  b->valid = 0;
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  acquiresleep(&b->lock);

  if ((buck + i)->next) {
    (buck + i)->next->prev = b;
  }
  b->next = (buck + i)->next;
  b->prev = buck + i;
  (buck + i)->next = b;

  release(&bulock[i]);

  return b;

  // for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
  //   if (b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;
  // printf("bread %d\n", blockno);

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  // printf("bwrite %d\n", b->blockno);
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int i = b->blockno % NBUC;
  // printf("brelse bulock %d\n", i);
  acquire(&bulock[i]);

  // printf("rel %d\n", b - bcache.buf);
  if (b->refcnt < 0) {
    panic("relse");
  }
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    if (b->next) {
      b->next->prev = b->prev;
    }
    b->prev->next = b->next;
    b->next = 0;
    b->prev = 0;

    acquire(&bcache.mlock);
    uint ind_ = b - bcache.buf;
    bcache.bmap |= (0x1 << ind_);
    release(&bcache.mlock);
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    //
    // release(&bulock[i]);
    // acquire(&bulock[NBUC]);
    // if ((buck + NBUC)->next) {
    //   (buck + NBUC)->next->prev = b;
    // }
    // b->next = (buck + NBUC)->next;
    // b->prev = buck + NBUC;
    // (buck + NBUC)->next = b;
    // release(&bulock[NBUC]);
    // return;
  }
  // printf("brelse bu unlock %d\n", i);
  release(&bulock[i]);
}

void bpin(struct buf *b) {

  int i = b->blockno % NBUC;
  // printf("bpin bulock %d\n", i);
  acquire(&bulock[i]);
  // acquire(&bcache.lock);
  b->refcnt++;

  // printf("bpin bu ublock %d\n", i);
  release(&bulock[i]);
  // release(&bcache.lock);
}

void bunpin(struct buf *b) {
  int i = b->blockno % NBUC;
  // printf("bunpin bulock %d\n", i);
  acquire(&bulock[i]);
  // acquire(&bcache.lock);
  b->refcnt--;
  // printf("bunpin bu unlock %d\n", i);
  release(&bulock[i]);
  // release(&bcache.lock);
}
