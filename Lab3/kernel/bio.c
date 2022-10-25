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


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

//lab3  哈希桶分组数，取13组
#define NBUCKETS 13 

//lab3 修改bcache
struct {
  // 每个哈希桶分配自旋锁
  //struct spinlock lock;
  struct spinlock lock[NBUCKETS];
  
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  // 每个哈希桶的头指针
  struct buf head[NBUCKETS];
} bcache;

// lab3  修改binit
void
binit(void)
{
  //struct buf *b;

  //initlock(&bcache.lock, "bcache");
  
  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  
  // 初始化每个哈希桶的锁与双向链表
  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.lock[i], "bcache");
    
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  
  
  
  //for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //  b->next = bcache.head.next;
  //  b->prev = &bcache.head;
  //  initsleeplock(&b->lock, "buffer");
  //  bcache.head.next->prev = b;
  //  bcache.head.next = b;
  //}
  
  // 以 i mod NBUCKETS为散列函数，将buf分组
  for(int i=0; i<NBUF; i++){
    bcache.buf[i].next = bcache.head[i % NBUCKETS].next;
    bcache.buf[i].prev = &bcache.head[i % NBUCKETS];
    
    initsleeplock(&(bcache.buf[i].lock), "buffer");
    
    bcache.head[i % NBUCKETS].next->prev = &bcache.buf[i];
    bcache.head[i % NBUCKETS].next = &bcache.buf[i];
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// lab3 修改bget
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // 获取所在哈希桶的锁
  //acquire(&bcache.lock);
  acquire(&bcache.lock[blockno % NBUCKETS]);
  
  // Is the block already cached?
  //for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //  if(b->dev == dev && b->blockno == blockno){
  //    b->refcnt++;
  //    release(&bcache.lock);
  //    acquiresleep(&b->lock);
  //    return b;
  //  }
  //}
  for(b = bcache.head[blockno % NBUCKETS].next; b != &bcache.head[blockno % NBUCKETS]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      
      release(&bcache.lock[blockno % NBUCKETS]);
      
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //  if(b->refcnt == 0) {
  //    b->dev = dev;
  //    b->blockno = blockno;
  //    b->valid = 0;
  //    b->refcnt = 1;
  //    release(&bcache.lock);
  //    acquiresleep(&b->lock);
  //    return b;
  //  }
  //}
  // 没有缓存则从所在哈希桶中获取空闲缓存
  for(b = bcache.head[blockno % NBUCKETS].prev; b != &bcache.head[blockno % NBUCKETS]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      
      release(&bcache.lock[blockno % NBUCKETS]);
      
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[blockno % NBUCKETS]);
  
  // 自己的哈希桶中获取失败，从其他哈希桶中获取
  for(int i=1;i<NBUCKETS;i++){
    int blockno_to_get = (blockno + i) % NBUCKETS;
    acquire(&bcache.lock[blockno_to_get]);
    for(b = bcache.head[blockno_to_get].next; b != &bcache.head[blockno_to_get]; b = b->next){
      if(b->refcnt == 0) {
        // 获取到缓存块b
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        
        // 从b所在哈希桶分离
        b->prev->next = b->next;
        b->next->prev = b->prev;
        
        // 加入原哈希桶
        acquire(&bcache.lock[blockno % NBUCKETS]);
        b->next = bcache.head[blockno % NBUCKETS].next;
        b->prev = &bcache.head[blockno % NBUCKETS];
        bcache.head[blockno % NBUCKETS].next->prev = b;
        bcache.head[blockno % NBUCKETS].next = b;
        release(&bcache.lock[blockno % NBUCKETS]);
        
        // 返回前释放b所在哈希桶的锁
        release(&bcache.lock[blockno_to_get]);
        
        acquiresleep(&b->lock);
        return b;
      }
    }
    // 目前寻找的哈希桶中无空闲缓存，释放锁
    release(&bcache.lock[blockno_to_get]);
  }
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
// lab3 修改brelse
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  
  // 缓存块b对应的哈希桶
  int num = b->blockno % NBUCKETS;
  
  //acquire(&bcache.lock);
  acquire(&bcache.lock[num]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
    b->next = bcache.head[num].next;
    b->prev = &bcache.head[num];
    bcache.head[num].next->prev = b;
    bcache.head[num].next = b;
  }
  //release(&bcache.lock);
  release(&bcache.lock[num]);
}

// lab3 修改bpin
void
bpin(struct buf *b) {
  // 缓存块b对应的哈希桶
  int num = b->blockno % NBUCKETS;
  //acquire(&bcache.lock);
  acquire(&bcache.lock[num]);
  b->refcnt++;
  //release(&bcache.lock);
  release(&bcache.lock[num]);
  
}

// lab3 修改bunpin
void
bunpin(struct buf *b) {
  // 缓存块b对应的哈希桶
  int num = b->blockno % NBUCKETS;
  //acquire(&bcache.lock);
  acquire(&bcache.lock[num]);
  b->refcnt--;
  //release(&bcache.lock);
  release(&bcache.lock[num]);
}


