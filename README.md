# HITSZ_OS_2022

## 实验概述

本实验为哈尔滨工业大学（深圳）2022秋季《操作系统》课程实验，前4个实验是从开源操作系统 **xv6-labs-2020** 的实验课程中选取4个实验，最后一个来自我校学生自主设计的文件系统实验案例。

实验代码框架： https://gitee.com/hitsz-lab/xv6-labs-2020.git

实验指导书： http://hitsz-cslab.gitee.io/os-labs/

本仓库内实验代码为个人作业，仅供参考，请勿抄袭。

## 仓库结构

- main分支：仅包括提交代码
- util分支：实验一完成后整体代码
- syscall分支：实验二完成后整体代码

## Lab1: XV6与UNIX实用程序

本次实验实现5个Unix实用程序，目的为掌握在xv6上编写用户程序的方法。

### sleep

本程序用于理解用户程序，只需在 ``Makefile`` 文件中找到 ``UPROGS`` ，并添加行``$U/_sleep\``

### pingpong

实现思路：父进程向管道1写入 ``ping`` 后，等待从管道2读出并打印；子进程从管道2读出后打印，接着向管道1写入 ``pong`` 。

### primes

函数 ``primes`` 接收一个管道p，从管道中读出第一个非0数即为质数，并以其为基数进行筛选；新建管道pp，调用fork创建子进程，父进程将后续从p读到的所有非0数除以基数，若整除则跳过，否则将其传入pp，子进程中递归调用 ``primes`` 并传入pp。主程序中父进程将2-35传入管道并在最后添加0作为结束符，子进程调用 ``primes`` ，即可实现筛选2-35中的质数。

### find

参考 ``ls.c`` ，修改其遍历至文件时直接打印为名称匹配才打印，遍历至文件夹时新增跳过 ``.`` 与 ``..`` ，然后递归调用find查找文件夹内所有文件和子目录。

### xargs

循环从标准输入中读取字符，若无读入或仅读到 ``\n`` 退出循环；每次读至 ``\n`` 并存在有效数据时，将所读字符串添加至参数列表并执行。

## Lab2: 系统调用

本节实验的目的是对操作系统的系统调用模块进行修改，尽可能在真正修改操系统之前，先对操作系统有一定的了解。

1. 了解xv6系统调用的工作原理。
2. 熟悉xv6通过系统调用给用户程序提供服务的机制。

### user

#### user.h

添加系统调用接口 ``trace`` 和 ``sysinfo`` 定义

```c
struct sysinfo;  // for sysinfo

// system calls
...
int trace(int mask);  // lab2
int sysinfo(struct sysinfo*);  // lab2
```

#### usys.pl

添加 ``trace`` 和 ``sysinfo`` 对应用户系统调用名称

```perl
entry("trace");  # lab2
entry("sysinfo");  # lab2 
```

### kernel

#### syscall.h

添加 ``trace`` 和 ``sysinfo`` 对应系统调用号

```c
// System call numbers
...
#define SYS_trace  22  // lab2
#define SYS_sysinfo 23  //lab2
#define SYS_CALL_AMOUNT 23
```

#### proc.h

为PCB结构体 ``proc`` 添加参数 ``mask``

```c
// Per-process state
struct proc {
  ...
  // lab2
  int mask;                    // syscall的参数mask
};
```

#### syscall.c

系统调用函数表中添加 ``trace`` 和 ``sysinfo`` 

```c
...
extern uint64 sys_trace(void);  // lab2
extern uint64 sys_info(void);  // lab2

static uint64 (*syscalls[])(void) = {
...
[SYS_trace]   sys_trace,  // lab2
[SYS_sysinfo] sys_info  // lab2
};
```

添加所有函数名数组，用于 ``trace`` 打印

```c
// lab2 所有系统调用函数名
char* syscall_name[SYS_CALL_AMOUNT] = {
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
  "sys_info"
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
    int arg;
    argint(0, &arg);
  
    p->trapframe->a0 = syscalls[num]();
    
    int mask = p->mask;
    // 判断mask的系统调用对应位是否为1
    if(mask & 1<<num){
      // 打印信息
      printf("%d: %s(%d) -> %d\n", p->pid, syscall_name[num-1], arg, p->trapframe->a0); 
    }
    
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}

```

#### kalloc.c

添加函数 ``check_freemem`` ，计算剩余的内存空间

```c
// lab2 sysinfo
// 计算剩余的内存空间
// 统计freelist长度，乘以PGSIZE即为空闲内存空间大小
int check_freemem(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  int freelist_num = 0;
  while(r)
  {
    r = r -> next;
    freelist_num++;
  }
  release(&kmem.lock);
  
  return freelist_num*PGSIZE;
}
```

#### proc.c

添加函数 ``check_nproc`` 与 ``check_nproc`` ，计算空闲进程数量与可用文件描述符数量

```c
// lab2 sysinfo
// 计算空闲进程数量
// 统计状态为UNUSED的进程数量即为所求
int check_nproc(void)
{
  struct proc *p;
  int num = 0;
  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p->state == UNUSED)
      num++;
  }

  return num;
}


// lab2 sysinfo
// 计算可用文件描述符数量
// 统计PCB成员ofile中为0的下标，即是空闲文件描述符
int check_freefd(void)
{
  struct proc *p = myproc();
  int i;
  int num = 0;

  for(i = 0; i < NOFILE; i++)
    if(!p->ofile[i])
      num++;
  
  return num;
}
```

#### defs.h

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

#### sysproc.c

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
uint64 sys_info(void)
{
  uint64 sysinfo_addr;
  // 获取*sysinfo参数失败，返回-1
  if(argaddr(0, &sysinfo_addr)<0){
    return -1;
  }
  
  struct proc *p = myproc();
  struct sysinfo info;
  info.freemem = check_freemem();  // 剩余内存空间
  info.nproc = check_nproc();  // 剩余空闲进程数量
  info.freefd = check_freefd();  // 剩余可用文件描述符
  // 将数据拷贝至用户空间
  if(copyout(p->pagetable, sysinfo_addr, (char *)&info, sizeof(info)) < 0){
    return -1;
  }
  
  return 0;
}
```

### Makefile

修改 ``UPROGS`` ，添加 ``$U/_trace\`` , ``$U/_sysinfotest\``。