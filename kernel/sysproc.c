#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"  // lab2 struct sysinfo

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// lab2 
// 系统调用trace
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