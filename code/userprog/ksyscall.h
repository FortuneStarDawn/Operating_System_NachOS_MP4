/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"

#include "synchconsole.h"


void SysHalt()
{
  kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

int SysCreate(char *filename, int initialSize)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename, initialSize);
}

OpenFileId SysOpen(char *name)
{
    return kernel->interrupt->Open(name);
}

//add SysWrite called by system call and call kernel through interrupt
int SysWrite(char *buffer, int size, OpenFileId id)
{
    return kernel->interrupt->Write(buffer, size, id);
}

//add SysRead called by system call and call kernel through interrupt
int SysRead(char *buffer, int size, OpenFileId id)
{
    return kernel->interrupt->Read(buffer, size, id);
}

//add SysClose called by system call and call kernel through interrupt
int SysClose(OpenFileId id)
{
    return kernel->interrupt->Close(id);
}

#endif /* ! __USERPROG_KSYSCALL_H__ */
