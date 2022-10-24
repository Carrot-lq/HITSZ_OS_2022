// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
  
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

// lab3
struct kmem kmems[NCPU];

// lab3 修改 kinit
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

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// lab3 修改 kfree
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

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// lab3 修改 kalloc
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