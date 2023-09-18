#include "date.h"
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "spinlock.h"
#include "types.h"

uint64 sys_sigalarm(void) {
  // printf("sigalarm1\n");
  struct proc *cur = myproc();
  // printf("sigalarm2\n");
  pte_t addr;
  int t;
  // printf("%p %p\n", cur->trapframe->a0, cur->trapframe->a1);
  // printf("%p %p\n", argint(0, &t), argaddr(1, &addr));
  if (argint(0, &t) < 0 || argaddr(1, &addr) < 0) {
    // printf("%d\n", cur->interval);
    return -1;
  }

  cur->interval = t;
  cur->handler = (void *)addr;
  // printf("alarm0\n");
  return 0;
}
uint64 sys_sigreturn(void) {
  struct proc *p = myproc();
  if (p->inlarm) {
    p->inlarm = 0;
    *p->trapframe = *p->alarmframe;
    p->pass_ticks = 0;
  } // printf("mv\n");
  return 0;
}

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64 sys_sleep(void) {
  backtrace();
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
