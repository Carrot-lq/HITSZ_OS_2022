# Lab3: 锁机制的应用

1. 了解Lock的实现原理，理解CPU为Lock的实现提供的支持。
2. 理解xv6的内存分配器kalloc以及磁盘缓存buffer cache的工作原理，掌握Lock在xv6中内存分配器/磁盘缓存中的作用。
3. 掌握锁竞争对程序并行性的影响。
4. 学习减少锁竞争的方法。

## 任务一：内存分配器

**修改内存分配器（主要修改kernel/kalloc.c）** ，使每个CPU核使用独立的内存链表，而不是现在的共享链表。

### kalloc.c

添加链表数组 ``kmems`` 

```c
struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

// lab3
struct kmem kmems[NCPU];
```

#### knit

修改为 ``cpuid`` 对应 ``kmem``

```c
void
kinit()
{
  //  initlock(&kmem.lock, "kmem");
  
  // 关闭中断
  push_off();
  // cpuid对应kmem
  int id = cpuid();
  initlock(&kmems[id].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  
  // 打开中断
  pop_off();
}
```

#### kfree

修改为 ``cpuid`` 对应 ``kmem``

```c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  
  r = (struct run*)pa;
  
  //acquire(&kmem.lock);
  //r->next = kmem.freelist;
  //kmem.freelist = r;
  //release(&kmem.lock);
  
  // 关闭中断
  push_off();
  
  int id = cpuid();
  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
  
  // 打开中断
  pop_off();
}
```

#### kalloc

首先尝试获取当前cpu自己的空闲内存链表，若无则依次去尝试获取下一个cpu的空闲链表

```c
void *
kalloc(void)
{
  struct run *r;
  
  //acquire(&kmem.lock);
  //r = kmem.freelist;
  //if(r)
  //  kmem.freelist = r->next;
  //release(&kmem.lock);
  
  // 关闭中断
  push_off();
  
  int id = cpuid();
  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if(r){
    kmems[id].freelist = r->next;
    release(&kmems[id].lock);
  }
  // 窃取其他cpu的内存
  else{
    release(&kmems[id].lock);
    // 从当前id向后依次尝试，最多遍历所有cpu
    for(int i=1;i<NCPU;i++){
      int id_to_get = (id + i) % NCPU;
      acquire(&kmems[id_to_get].lock);
      r = kmems[id_to_get].freelist;
      if(r){
        // 成功获取即可退出
        kmems[id_to_get].freelist = r->next;
        release(&kmems[id_to_get].lock);
        break;
      }
      // 窃取失败，解除对应锁
      release(&kmems[id_to_get].lock);
    }
  }

  // 打开中断
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

## 任务二：磁盘缓存

**修改磁盘缓存块列表的管理机制（主要修改kernel/bio.c）** ，使得可用多个锁管理缓存块，从而减少缓存块管理的锁争用。

### bio.c

哈希桶分组数，取13组

```c
#define NBUCKETS 13 
```

每个哈希桶分配自旋锁

```c
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
```

#### binit

为每个哈希桶初始化锁和双向链表。

然后以模 ``NBUCKETS`` 为散列函数，将 ``buf`` 分组至每个哈希桶。

```c
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
```

#### bget

首先查看是否存在已缓存的块。

若未缓存，先在对应哈希桶中寻找空闲缓存块。

若本哈希桶中无空闲，则从下一个哈希桶开始遍历所有哈希桶并尝试获取空闲缓存块。

若仍未获取到则报错退出。

```c
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
```

#### brelse

释放缓存块b时，在相应哈希桶中操作。

```c
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
```

#### bpin

使用的锁修改为对应哈希桶拥有的锁。

```c
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
```

#### bunpin

同上

```c
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
```
