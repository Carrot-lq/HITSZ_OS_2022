# Lab4: 页表

1. 了解页表的实现原理。
2. 修改页表，使内核更方便的进行用户虚拟地址翻译。

## 任务一：打印页表

参照 ``freewalk`` 遍历页表的逻辑，在递归时实现打印。

### vm.c

新增函数``vmprint`` ，其中通过调用递归函数 ``printwalk`` （参照 ``freewalk`` ）逐层打印信息。

打印的格式为 ``$index: pte $pte_bits pa $physical_address`` ，其中：

- ``index``表示标号
-  ``pte_bits`` 表示页表项的十六进制值
-  ``physical_address`` 表示页表项对应的十六进制物理地址。

```c
// lab4 task1 vmprint
// 参照freewalk遍历页表，递归打印信息
void 
printwalk(pagetable_t pagetable, int depth)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      // 打印信息
      for(int i=depth;i>1;i--)
        printf("|| ");
      printf("||");
      printf("%d: pte %p pa %p\n", i, pte, child);
      printwalk((pagetable_t)child, depth+1);
    } else if(pte & PTE_V){
      uint64 child = PTE2PA(pte);
      // 打印信息
      for(int i=depth;i>1;i--)
        printf("|| ");
      printf("||");
      printf("%d: pte %p pa %p\n", i, pte, child);
    }
  }
}

void 
vmprint(pagetable_t pagetable)
{
    printf("page table %p\n", pagetable);
    printwalk(pagetable, 1);
}
```

并在 ``defs.h`` 中添加函数 ``vmprint`` 的声明。

```c
// vm.c
...
void            vmprint(pagetable_t);// lab4 task1 打印页表
```

### exec.c

添加使用 ``vmprint`` 语句。

```c
int exec(char *path, char **argv){
    ...
  // lab4 task1
  // 在exec()返回argc之前执行vmprint
  if(p->pid==1) vmprint(p->pagetable);
  
  return argc; // this ends up in a0, the first argument to main(argc, argv)
    ...
}
```

## 任务二：独立内核页表

**将共享内核页表改成独立内核页表** ，使得每个进程拥有自己独立的内核页表。

首先在PCB中增加两个新成员代表内核独立页表和内核栈的物理地址，然后修改分配、回收、切换内核页表的代码逻辑，大致包括 ``procinit()`` 、 ``allocproc()`` 、 ``scheduler`` ， ``freeproc()`` 等。

### proc.h

修改 ``proc`` 结构体，新增两个属性：

- ``pagetable_t k_pagetable`` ：内核独立页表
- ``uint64 kstack_pa`` ：内核栈的物理地址

```c
// Per-process state
struct proc {
  struct spinlock lock;
	...
  char name[16];               // Process name (debugging)
  // lab4 task2 独立内核页表
  pagetable_t k_pagetable;     // 内核独立页表
  uint64 kstack_pa;            // 内核栈物理地址
};
```

### vm.c

新建一系列函数，提供为每个进程独立内核页表的相关操作。

#### proc_kvmmap

同 ``kvmmap`` 函数，新增参数``pagetable_t proc_kernel_pagetable`` ，改为指定内核页表而非唯一的全局内核页表。

```c
// lab4 task2 为独立页表建立PTE
void
proc_kvmmap(pagetable_t proc_kernel_pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(proc_kernel_pagetable, va, sz, pa, perm) != 0)
    panic("proc_kvmmap");
}
```

#### <a name="proc_kvminit">proc_kvminit</a>

同 ``kvminit`` 函数，创建进程内核页表，其中映射时调用新的 ``proc_kvmmap`` 代替 ``kvmmap`` 函数，并将新建的页表作为返回值返回。

并依据指导书，不映射 ``CLINT`` 。

```c
// lab4 task2 仿照kvminit，创建内核页表
pagetable_t
proc_kvminit()
{  
  pagetable_t proc_kernel_pagetable = (pagetable_t)kalloc();
  memset(proc_kernel_pagetable, 0, PGSIZE);

  // uart registers
  proc_kvmmap(proc_kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  proc_kvmmap(proc_kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT 不映射
  //proc_kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  
  // PLIC
  proc_kvmmap(proc_kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  proc_kvmmap(proc_kernel_pagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  proc_kvmmap(proc_kernel_pagetable, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  proc_kvmmap(proc_kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  
  return proc_kernel_pagetable;
}
```

#### <a name="proc_kvminithart">proc_kvminithart</a>

同 ``kvminithart`` 函数，修改为切换到进程的独立内核页表，而非唯一的全局内核页表。

```c
// lab4 task2 切换到进程的独立内核页表
void
proc_kvminithart(pagetable_t proc_kernel_pagetable)
{
  w_satp(MAKE_SATP(proc_kernel_pagetable));
  sfence_vma();
}
```

#### <a name="proc_freewalk">proc_freewalk</a>

释放页表但不释放叶子页表指向的物理页帧，即在递归至叶子节点时不再向下递归即可。

```c
// lab4 task2 
// 释放页表但不释放叶子页表指向的物理页帧
void
proc_freewalk(pagetable_t pagetable)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // 如果该页表项指向更低一级的页表
      // 递归释放低一级页表及其页表项
      uint64 child = PTE2PA(pte);
      proc_freewalk((pagetable_t)child);
      pagetable[i] = 0;  
    } else if(pte & PTE_V){
      // 叶子页表不再递归向下
      pagetable[i] = 0;
    }
  }
  kfree((void*)pagetable);
}
```

### proc.c

#### procinit

初始化时将内核栈物理地址 ``pa`` 存储到PCB的成员 ``kstack_pa`` ，并暂时保留内核栈在全局页表 ``kernel_pagetable`` 的映射（即对 ``kvmmap()`` 的调用），在 ``allocproc()`` 中再把内核栈映射到进程的独立内核页表里（使用新建的 ``proc_kvmmap()`` ）。

```c
// initialize the proc table at boot time.
// lab4 task2 修改procinit
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");

      // Allocate a page for the process's kernel stack.
      // Map it high in memory, followed by an invalid
      // guard page.
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      // lab4 task2
      // 内核栈物理地址存储到PCB
      p->kstack_pa = (uint64)pa;
      // 保留内核栈在全局页表kernel_pagetable的映射
      // 在allocproc()中再把内核栈映射到进程的独立内核页表里
      uint64 va = KSTACK((int) (p - proc));
      kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);  
      p->kstack = va;
  }
  kvminithart();
}
```

#### allocproc

新增创建内核页表（见 [proc_kvminit](#proc_kvminit)）并建立内核栈映射（ ``proc_kvmmap()``）的逻辑。

```c
// lab4 task2 修改allocproc 
static struct proc*
allocproc(void)
{
	...
  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  
  // lab4 task2 
  // 初始化进程的独立内核页表
  p->k_pagetable = (pagetable_t)proc_kvminit();
  if(p->k_pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  // 将p的内核栈虚拟地址映射到独立内核页表
  proc_kvmmap(p->k_pagetable, p->kstack, p->kstack_pa, PGSIZE, PTE_R | PTE_W);

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}
```

#### freeproc

增加释放进程的独立内核页表（见 [proc_freewalk](#proc_freewalk)）。

```c
//lab4 task2 修改freeproc
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  // lab4 task2 
  // 释放独立内核页表
  if(p->k_pagetable)
    proc_freewalk(p->k_pagetable);
  p->k_pagetable = 0;
	...
}
```

#### scheduler

调度器在切换进程的时候将进程的内核页表调入 ``satp`` 寄存器（见 [proc_kvminithart](#proc_kvminithart)），进程结束时切换回全局内核页表（``kvminithart()``）。

```c
// lab4 task2 修改scheduler
void
scheduler(void)
{
	...
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        // lab4 task2 
        // 调入进程的页表
        proc_kvminithart(p->k_pagetable);
        swtch(&c->context, &p->context);
        // 进程结束后切换回内核页表
        kvminithart();
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0; // cpu dosen't run any process now

        found = 1;
      }
      release(&p->lock);
    }
   	...
}
```

## 任务三：简化软件模拟地址翻译

**在独立内核页表加上用户页表的映射，同时替换 `copyin()/copyinstr()` 为 `copyin_new()/copyinstr_new()`** ，使得内核能够不必花费大量时间，用软件模拟的方法一步一步遍历页表，而是直接利用硬件。

需要在独立内核页表加上用户页表的映射，以保证替换的新函数能够使用。且需注意独立内核页表的用户页表的映射的标志位的选择。标志位User一旦被设置，内核就不能访问该虚拟地址了。

每一次用户页表被修改了映射的同时，都要修改对应独立内核页表的相应部分以保持同步。

需要修改 ``fork()`` 、 ``exec()`` ， ``growproc()`` 等，实现将改变后的进程页表同步到内核页表中。另外 ``userinit()`` 中，对于第一个进程也需要将用户页表映射到内核页表。

### vm.c

首先将 ``copyin()`` 与 ``copyinstr()`` 替换。

#### copyin / copyinstr

```c
// lab4 task3 用copyin_new代替copyin
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable, dst, srcva, len);
}

// lab4 task3 用copyinstr_new代替copyinstr
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  return copyinstr_new(pagetable, dst, srcva, max);
}
```

#### vmcopypage

新建函数 ``vmcopypage()`` ，其作用为将给定进程的用户页表映射到给定内核页表中。

```c
// lab4 task3 复制用户页表至内核页表
void
vmcopypage(pagetable_t pagetable, pagetable_t k_pagetable, uint64 start, uint64 sz)
{
  for(uint64 i=start;i<start+sz;i+=PGSIZE){
    pte_t *pte = walk(pagetable, i, 0);
    pte_t *kpte = walk(k_pagetable, i, 1);
    if(!pte || !kpte)
      panic("vmcopypage");
    *kpte = (*pte) & ~(PTE_U | PTE_W | PTE_X);
  }
}
```

### proc.c

若干函数中需要同步进程页表与内核页表。

#### userinit

第一个进程需要将用户页表映射到内核页表中，通过调用 ``vmcopypage`` 实现。

```c
// lab4 task3 修改userinit
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;
  
  // lab4 task3 初始化时需要复制用户页表到内核页表
  vmcopypage(p->pagetable, p->k_pagetable, 0, p->sz);
  
  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}
```

#### fork

``fork`` 中需要调用 ``vmcopypage`` ，将生成的子进程的页表复制到其内核页表中。

```c
// lab4 task3 修改fork
int
fork(void)
{
	...
  np->state = RUNNABLE;

  // lab4 task3 将子进程的页表复制到内核页表上
  vmcopypage(np->pagetable, np->k_pagetable, 0, np->sz);
  
  release(&np->lock);

  return pid;
}
```

#### growproc

内存空间增加或减少时，对应新建或解除相应空间的映射。

并限制内存增加不得超出范围。

```c
// lab4 task3 修改growproc
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();
  
  sz = p->sz;
  
  // 限制内存范围
  if(sz + n >= PLIC)
    return -1;
    
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
    // 内存增加时复制该部分
    vmcopypage(p->pagetable, p->k_pagetable, sz - n, n);
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
    
    // 内存减少时解除映射
    for(uint64 j=PGROUNDUP(sz); j<PGROUNDUP(sz-n);j+=PGSIZE){
      uvmunmap(p->k_pagetable, j, 1, 0);
    }
  }
  p->sz = sz;
  return 0;
}
```

### exec.c

#### exec

调用 ``uvmunmap()`` 释放旧内核页表的映射，然后调用 ``vmcopypage`` 将新的进程页表复制到内核页表中。

```c
int
exec(char *path, char **argv)
{
	...    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);
  
  // lab4 task3 释放旧内核页表的映射
  uvmunmap(p->k_pagetable, 0, PGROUNDUP(oldsz)/PGSIZE, 0);
  // 并将进程页表的映射复制到进程的内核页表
  vmcopypage(p->pagetable, p->k_pagetable, 0, p->sz);
  
  // lab4 task1
  // 在exec()返回argc之前执行vmprint
  if(p->pid==1) vmprint(p->pagetable);
  
  return argc; // this ends up in a0, the first argument to main(argc, argv)
	...
}
```