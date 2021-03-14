#ifndef KERN_VM_H
#define KERN_VM_H

#include <stdint.h>
#include "proc.h"

// static uint64_t* pgdir_walk(uint64_t *,const void *, int64_t) ;
// static int map_region(uint64_t *, void *, uint64_t , uint64_t , int64_t) ;
void vm_free(uint64_t*, int);
void test_code();
void uvm_switch(struct proc*);
void uvm_init(uint64_t*, char*, int);
int allocuvm(uint64_t* pgdir, uint32_t oldsz, uint32_t newsz);
int deallocuvm(uint64_t* pgdir, uint32_t oldsz, uint32_t newsz);
int loaduvm(uint64_t* pgdir, char* addr, struct inode* ip, uint32_t offset, uint32_t sz);
void clearpteu(uint64_t* pgdir, char* uva);
char* uva2ka(uint64_t* pgdir, char* uva);
int copyout(uint64_t* pgdir, uint32_t va, void* p, uint32_t len);
uint64_t* copyuvm(uint64_t* pgdir, uint32_t sz);

uint64_t* pgdir_init();

#endif