#include <stdint.h>
#include "types.h"
#include "mmu.h"
#include "string.h"
#include "memlayout.h"
#include "console.h"

#include "vm.h"
#include "kalloc.h"
#include "proc.h"
#include "file.h"

extern uint64_t* kpgdir;

/*
 * Given 'pgdir', a pointer to a page directory, pgdir_walk returns
 * a pointer to the page table entry (PTE) for virtual address 'va'.
 * This requires walking the four-level page table structure.
 *
 * The relevant page table page might not exist yet.
 * If this is true, and alloc == false, then pgdir_walk returns NULL.
 * Otherwise, pgdir_walk allocates a new page table page with kalloc.
 *   - If the allocation fails, pgdir_walk returns NULL.
 *   - Otherwise, the new page is cleared, and pgdir_walk returns
 *     a pointer into the new page table page.
 */

 /*
  * 47..63 --
  * 39..47 -- 9 bits of level-0 index.
  * 30..38 -- 9 bits of level-1 index.
  * 21..29 -- 9 bits of level-2 index.
  * 12..20 -- 9 bits of level-3 index.
  *  0..11 -- 12 bits of byte offset within the page.
 */
static uint64_t*
pgdir_walk(uint64_t* pgdir, const void* va, int64_t alloc)
{
    /* TODO: Your code here. */

    if ((uint64_t)va >= ((uint64_t)1 << (9 + 9 + 9 + 9 + 12 - 1)))
        panic("pgdir_walk");

    for (int level = 0; level < 3; level++) {
        uint64_t* pte = &pgdir[PTX(level, va)];
        if (*pte & PTE_P) {//relevant pagetable exists
            pgdir = (uint64_t*)P2V(PTE_ADDR(*pte));
        }
        else {
            if (!alloc || (pgdir = (uint64_t*)kalloc()) == 0)
                return NULL;
            memset(pgdir, 0, PGSIZE);
            *pte = V2P(pgdir) | PTE_P | PTE_TABLE;
        }
    }
    return &pgdir[PTX(3, va)];
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might **NOT**
 * be page-aligned.
 * Use permission bits perm|PTE_P|PTE_TABLE|(MT_NORMAL << 2)|PTE_AF|PTE_SH for the entries.
 *
 * Hint: call pgdir_walk to get the corresponding page table entry
 */
static int
map_region(uint64_t* pgdir, void* va, uint64_t size, uint64_t pa, int64_t perm)
{
    /* TODO: Your code here. */
    uint64_t* begin, * end;
    uint64_t* pte;
    begin = (uint64_t*)PTE_ADDR(va);
    end = (uint64_t*)PTE_ADDR(va + size - 1);
    while (1) {

        if ((pte = pgdir_walk(pgdir, begin, 1)) == 0)//let pte be the pointer of the end of the table page
            return -1;
        if (*pte & PTE_P)
            panic("remap");
        *pte = PTE_ADDR(pa) | perm | PTE_P | PTE_TABLE | (MT_NORMAL << 2) | PTE_AF | PTE_SH;//give pte value
        if (begin == end)
            break;
        begin += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

/*
 * Free a page table.
 *
 * Hint: You need to free all existing PTEs for this pgdir.
 */

void
vm_free(uint64_t* pgdir, int level)
{
    /* TODO: Your code here. */
    for (int i = 0; i < 512; i++) {
        uint64_t pte = pgdir[i];
        if (pte & PTE_P) {
            if (level < 3)
                vm_free((uint64_t*)(P2V(PTE_ADDR(pte))), level + 1);
            else if (level == 3) {
                kfree((char*)P2V(PTE_ADDR(pte)));
            }
        }
        // pgdir[i] = 0;

    }
    if (level < 3)
        kfree((char*)pgdir);

    return;
}


/* Get a new page table */
uint64_t*
pgdir_init()
{
    /* TODO: Your code here. */
    uint64_t* pagetable;
    pagetable = (uint64_t*)kalloc();
    if (pagetable == NULL) {
        panic("pgdir_init: cannot alloc a page");
        return 0;
    }

    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

/*
 * Load binary code into address 0 of pgdir.
 * sz must be less than a page.
 * The page table entry should be set with
 * additional PTE_USER|PTE_RW|PTE_PAGE permission
 */
void
uvm_init(uint64_t* pgdir, char* binary, int sz)
{
    /* TODO: Your code here. */
    char* mem;

    if (sz >= PGSIZE)
        panic("inituvm: more than a page");

    mem = kalloc();
    if (mem == NULL) {
        panic("uvm_init: cannot alloc a page");
    }
    memset(mem, 0, PGSIZE);
    map_region(pgdir, 0, PGSIZE, V2P(mem), PTE_RW | PTE_USER | PTE_PAGE);
    memmove(mem, binary, sz);
}

/*
 * switch to the process's own page table for execution of it
 */
void
uvm_switch(struct proc* p)
{
    /* TODO: Your code here. */
    if (p->pgdir == NULL) {
        panic("switchuvm: no pgdir");
    }
    lttbr0(V2P(p->pgdir));
}


int allocuvm(uint64_t* pgdir, uint32_t oldsz, uint32_t newsz)
{
    char* mem;
    uint64_t a;

    if (newsz >= UADDR_SZ) {
        return 0;
    }

    if (newsz < oldsz) {
        return oldsz;
    }

    a = ROUNDUP(oldsz, PGSIZE);

    for (; a < newsz; a += PGSIZE) {
        mem = kalloc();

        if (mem == 0) {
            cprintf("allocuvm out of memory\n");
            deallocuvm(pgdir, newsz, oldsz);
            return 0;
        }

        memset(mem, 0, PGSIZE);
        map_region(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_USER);
    }

    return newsz;
}


int deallocuvm(uint64_t* pgdir, uint32_t oldsz, uint32_t newsz)
{
    uint64_t* pte;
    uint64_t a;
    uint32_t pa;

    if (newsz >= oldsz) {
        return oldsz;
    }

    for (a = ROUNDUP(newsz, PGSIZE); a < oldsz; a += PGSIZE) {
        pte = pgdir_walk(pgdir, (char*)a, 0);

        if (!pte) {
            // pte == 0 --> no page table for this entry
            // round it up to the next page directory
            a = ROUNDUP(a, BKSIZE);

        }
        else if ((*pte & (PTE_PAGE | PTE_P)) != 0) {
            pa = PTE_ADDR(*pte);

            if (pa == 0) {
                panic("deallocuvm");
            }

            kfree(P2V(pa));
            *pte = 0;
        }
    }

    return newsz;
}

int loaduvm(uint64_t* pgdir, char* addr, struct inode* ip, uint32_t offset, uint32_t sz)
{
    uint64_t i, pa, n, va, start;
    uint64_t* pte;

    if ((uint64_t)(addr - offset) % PGSIZE != 0) {
        panic("loaduvm: addr must be page aligned");
    }
    //
    va = ROUNDDOWN((uint64_t)addr, PGSIZE);
    pte = pgdir_walk(pgdir, va, 0);
    if (pte == 0) {
        panic("loaduvm: addr 0x%p should exist\n", va);
    }
    pa = PTE_ADDR(*pte);
    start = (uint64_t)addr % PGSIZE;
    n = MIN(sz, PGSIZE - start);
    if (readi(ip, P2V(pa + start), offset, n) != n) {
        return -1;
    }
    offset += n;
    addr += n;
    sz -= n;
    //
    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = pgdir_walk(pgdir, addr + i, 0)) == 0) {
            panic("loaduvm: address should exist");
        }
        pa = PTE_ADDR(*pte);
        if (sz - i < PGSIZE) {
            n = sz - i;
        }
        else {
            n = PGSIZE;
        }
        if (readi(ip, P2V(pa), offset + i, n) != n) {
            return -1;
        }
    }

    return 0;
}

void clearpteu(uint64_t* pgdir, char* uva)
{
    uint64_t* pte;

    pte = pgdir_walk(pgdir, uva, 0);
    if (pte == 0) {
        panic("clearpteu");
    }

    // in ARM, we change the AP field (ap & 0x3) << 4)
    *pte = (*pte & ~(PTE_USER | PTE_RO)) | PTE_RW;
}

char* uva2ka(uint64_t* pgdir, char* uva)
{
    uint64_t* pte;

    pte = pgdir_walk(pgdir, uva, 0);

    // make sure it exists
    if ((*pte & (PTE_TABLE | PTE_P)) == 0) {
        return 0;
    }

    // make sure it is a user page
    if ((!(*pte & PTE_USER)) || (*pte & PTE_RO)) {
        return 0;
    }

    return (char*)P2V(PTE_ADDR(*pte));
}

int copyout(uint64_t* pgdir, uint32_t va, void* p, uint32_t len)
{
    char* buf, * pa0;
    uint64_t n, va0;

    buf = (char*)p;

    while (len > 0) {
        va0 = ROUNDDOWN(va, PGSIZE);
        pa0 = uva2ka(pgdir, (char*)va0);

        if (pa0 == 0) {
            return -1;
        }

        n = PGSIZE - (va - va0);

        if (n > len) {
            n = len;
        }

        memmove(pa0 + (va - va0), buf, n);

        len -= n;
        buf += n;
        va = va0 + PGSIZE;
    }

    return 0;
}

uint64_t* copyuvm(uint64_t* pgdir, uint32_t sz)
{
    uint64_t* d;
    uint64_t* pte;
    uint64_t pa, i, ap;
    char* mem;

    // allocate a new first level page directory
    d = pgdir_init();
    if (d == NULL) {
        return NULL;
    }

    // copy the whole address space over (no COW)
    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = pgdir_walk(pgdir, (void*)i, 0)) == 0) {
            panic("copyuvm: pte should exist");
        }

        if (!(*pte & (PTE_PAGE | PTE_P))) {
            panic("copyuvm: page not present");
        }

        pa = PTE_ADDR(*pte);
        ap = *pte & (PTE_USER | PTE_RO);//not sure for the PTE_AP

        if ((mem = kalloc()) == 0) {
            goto bad;
        }

        memmove(mem, (char*)P2V(pa), PGSIZE);

        if (map_region(d, (void*)i, PGSIZE, V2P(mem), ap) < 0) {
            goto bad;
        }
    }
    return d;

bad: kfree((char*)d);
    return 0;
}
