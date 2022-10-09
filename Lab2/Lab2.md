# Lab2: 系统调用

本节实验的目的是对操作系统的系统调用模块进行修改，尽可能在真正修改操系统之前，先对操作系统有一定的了解。

1. 了解xv6系统调用的工作原理。
2. 熟悉xv6通过系统调用给用户程序提供服务的机制。

## user

### user.h

添加系统调用接口 ``trace`` 和 ``sysinfo`` 定义

```c
struct sysinfo;  // for sysinfo

// system calls
...
int trace(int mask);  // lab2
int sysinfo(struct sysinfo*);  // lab2
```

### usys.pl

添加 ``trace`` 和 ``sysinfo`` 对应用户系统调用名称

```perl
entry("trace");  # lab2
entry("sysinfo");  # lab2 
```

## kernel

### syscall.h

添加 ``trace`` 和 ``sysinfo`` 对应系统调用号

```c
// System call numbers
...
#define SYS_trace  22  // lab2
#define SYS_sysinfo 23  //lab2
#define SYSCALL_TOTAL_AMOUNT 23
```

### proc.h

为PCB结构体 ``proc`` 添加参数 ``mask``

```c
// Per-process state
struct proc {
  ...
  // lab2
  int mask;                    // syscall的参数mask
};
```

### syscall.c

系统调用函数表中添加 ``trace`` 和 ``sysinfo`` 

```c
...
extern uint64 sys_trace(void);  // lab2
extern uint64 sys_sysinfo(void);  // lab2

static uint64 (*syscalls[])(void) = {
...
[SYS_trace]   sys_trace,  // lab2
[SYS_sysinfo] sys_sysinfo  // lab2
};
```

添加所有函数名数组，用于 ``trace`` 打印

```c
// lab2 所有系统调用函数名
char* syscall_name[SYSCALL_TOTAL_AMOUNT] = {
  "sys_fork",
  "sys_exit",
  "sys_wait",
  "sys_pipe",
  "sys_read",
  "sys_kill",
  "sys_exec",
  "sys_fstat",
  "sys_chdir",
  "sys_dup",
  "sys_getpid",
  "sys_sbrk",
  "sys_sleep",
  "sys_uptime",
  "sys_open",
  "sys_write",
  "sys_mknod",
  "sys_unlink",
  "sys_link",
  "sys_mkdir",
  "sys_close",
  "sys_trace",
  "sys_sysinfo"
};
```

修改 ``syscall`` ，实现打印信息

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // lab2 修改系统调用syscall实现trace打印信息
    // 获取第一个参数
    int arg0;
    argint(0, &arg0);
    
    p->trapframe->a0 = syscalls[num]();
    
    // 判断mask的系统调用对应位是否为1
    if(p->mask & 1<<num){
      // 打印信息，格式
      //PID: sys_$name(arg0) -> return_value
      printf("%d: %s(%d) -> %d\n", p->pid, syscall_name[num-1], arg0, p->trapframe->a0); 
    }
    
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

### kalloc.c

添加函数 ``check_freemem`` ，计算剩余的内存空间

```c
// lab2 sysinfo
// 计算剩余的内存空间
// 统计freelist长度，乘以PGSIZE即为空闲内存空间大小
int check_freemem(void)
{
  struct run *r;
  int freelist_num = 0;
  
  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r)
  {
    r = r -> next;
    freelist_num++;
  }
  release(&kmem.lock);
  
  return freelist_num*PGSIZE;
}
```

### proc.c

添加函数 ``check_nproc`` 与 ``check_nproc`` ，计算空闲进程数量与可用文件描述符数量

```c
// lab2 sysinfo
// 计算空闲进程数量
// 统计状态为UNUSED的进程数量即为所求
int check_nproc(void)
{
  int nproc_num = 0;
  
  for(int i=0;i<NPROC;i++)
  {
    if(proc[i].state == UNUSED)
      nproc_num++;
  }

  return nproc_num;
}


// lab2 sysinfo
// 计算可用文件描述符数量
// 统计PCB成员ofile中为0的下标，即是空闲文件描述符
int check_freefd(void)
{
  struct proc *p = myproc();
  int freefd_num = 0;

  for(int i=0;i<NOFILE;i++){
    if(!p->ofile[i]){
      freefd_num++;
    }
  }
    
  return freefd_num;
}
```

### defs.h

在kalloc.c与proc.c对应区域添加新增的函数。

```c
// kalloc.c
...
int             check_freemem(void);   // lab2 sysinfo

// proc.c
...
int             check_nproc(void);         // lab2 sysinfo
int             check_freefd(void);        // lab2 sysinfo
```

### sysproc.c

添加 ``sys_trace`` 与 ``sys_info`` 的实现

```c
#include "sysinfo.h"  // lab2 struct sysinfo
...
// lab2 
// 系统调用trace
// 设置待追踪的系统调用位
uint64 sys_trace(void)
{
    int mask;
    // 获取mask参数失败，返回-1
    if(argint(0, &mask) < 0){
      return -1;
    }
    // 设置mask
    struct proc *p = myproc();
    p->mask = mask;
    
    return 0;
}

// lab2
// 系统调用sysinfo
// 打印进程相关信息
uint64 sys_sysinfo(void)
{
  uint64 sysinfo_addr;
  // 获取*sysinfo参数失败，返回-1
  if(argaddr(0, &sysinfo_addr)<0){
    return -1;
  }
  //收集sysinfo
  struct sysinfo info;
  info.freemem = check_freemem();  // 剩余内存空间
  info.nproc = check_nproc();  // 剩余空闲进程数量
  info.freefd = check_freefd();  // 剩余可用文件描述符
  // 将数据拷贝至用户空间
  struct proc *p = myproc();
  if(copyout(p->pagetable, sysinfo_addr, (char *)&info, sizeof(info)) < 0){
    return -1;
  }
  
  return 0;
}
```

## Makefile

修改 ``UPROGS`` ，添加 ``$U/_trace\`` , ``$U/_sysinfotest\``。