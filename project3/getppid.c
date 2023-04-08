#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


int getppid(void){
	struct proc * parproc = myproc()->parent;
	return parproc->pid;
}
int sys_getppid(void){
	if(myproc()->parent->pid <0)
		return -1;
	return getppid();
}
