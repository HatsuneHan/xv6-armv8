#include <elf.h>

#include "trap.h"

#include "file.h"
#include "log.h"
#include "string.h"

#include "console.h"
#include "vm.h"
#include "proc.h"
#include "memlayout.h"
#include "syscallno.h"
#include "mmu.h"
#include "kalloc.h"

int
execve(const char* path, char* const argv[], char* const envp[])
{


    /* TODO: Load program into memory. */
    Elf64_Ehdr elf;
    struct inode* ip;
    Elf64_Phdr ph;
    uint64_t* pgdir, * oldpgdir;
    char* s, * last;
    int i, off;
    uint64_t argc, sz, sp, ustack[3 + MAXARG + 1], tmp;
    struct proc* curproc = thisproc();

    // begin_op();
    if ((ip = namei(path)) == 0) {
        // end_op();
        cprintf("exec: fail\n");
        return -1;
    }

    ilock(ip);
    pgdir = 0;

    if (readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf))
        goto bad;

    if (strncmp((const char*)elf.e_ident, ELFMAG, 4)) {
        cprintf("exec: magic not match, path %s\n", path);
        goto bad;
    }
    // if (elf.e_ident != ELFMAG) {
    //     cprintf("exec: magic not match, path %s\n", path);
    //     goto bad;
    // }

    if ((pgdir = pgdir_init()) == 0)
        goto bad;

    sz = 0;

    for (i = 0, off = elf.e_phoff;i < elf.e_phnum;i++, off += sizeof(ph)) {

        if (readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;

        if (ph.p_type != PT_LOAD)
            continue;
        if (ph.p_memsz < ph.p_filesz)
            goto bad;
        if ((sz = allocuvm(pgdir, sz, ph.p_vaddr + ph.p_memsz)) == 0)
            goto bad;

        if (loaduvm(pgdir, (char*)ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) < 0)
            goto bad;

    }

    iunlockput(ip);
    // end_op();
    ip = 0;


    sz = ROUNDUP(sz, PGSIZE);
    if ((sz = allocuvm(pgdir, sz, sz + 2 * PGSIZE)) == 0)
        goto bad;

    clearpteu(pgdir, (char*)(sz - (PGSIZE << 1)));
    sp = sz;

    // must complete copyout
    for (argc = 0; argv[argc]; argc++) {
        if (argc > MAXARG)
            goto bad;
        sp = sp - strlen(argv[argc]) + 1;
        sp = ROUNDDOWN(sp, 16);
        if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
            goto bad;
        ustack[argc] = sp;
    }
    ustack[argc] = 0;

    // in ARM, parameters are passed in r0 and r1
    curproc->tf->x0 = argc;

    if ((argc & 1) == 0) {
        sp -= 8;
    }
    uint64_t auxv[] = { 0, AT_PAGESZ, PGSIZE, AT_NULL };
    sp -= sizeof(auxv);
    // sp = ROUNDDOWN(sp, 16);
    if (copyout(pgdir, sp, auxv, sizeof(auxv)) < 0) {
        goto bad;
    }
    // skip envp
    sp -= 8;
    uint64_t temp = 0;
    if (copyout(pgdir, sp, &temp, 8) < 0) {
        goto bad;
    }



    // if (((sp - (argc + 1) * 8) & 0xf) == 0) {
    //     argc++;
    //     ustack[argc] = 0;
    // }

    curproc->tf->x1 = sp - (argc + 1) * 8;
    sp -= (argc + 1) * 8;
    if (copyout(pgdir, sp, ustack, (argc + 1) * 8) < 0) {
        goto bad;
    }

    sp -= 8;
    if (copyout(pgdir, sp, &thisproc()->tf->x0, 8) < 0) {
        goto bad;
    }

    // strncpy(curproc->name, last, sizeof(curproc->name));

    // Commit to the user image.
    oldpgdir = curproc->pgdir;
    curproc->pgdir = pgdir;
    curproc->sz = sz;
    // sp = ROUNDDOWN(sp, 16);
    curproc->tf->ELR_EL1 = elf.e_entry;
    curproc->tf->SP_EL0 = sp;

    uvm_switch(curproc);
    vm_free(oldpgdir, 1);
    return curproc->tf->x0;

bad:
    cprintf("\nbad");
    if (pgdir)
        vm_free(pgdir, 0);
    if (ip) {
        iunlockput(ip);
        // end_op();
    }
    return -1;
    /* TODO: Allocate user stack. */

    /* TODO: Push argument strings.
     *
     * The initial stack is like
     *
     *   +-------------+
     *   | auxv[o] = 0 |
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   auxv[0]   |
     *   +-------------+
     *   | envp[m] = 0 |
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   envp[0]   |
     *   +-------------+
     *   | argv[n] = 0 |  n == argc
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   argv[0]   |
     *   +-------------+
     *   |    argc     |
     *   +-------------+  <== sp
     *
     * where argv[i], envp[j] are 8-byte pointers and auxv[k] are
     * called auxiliary vectors, which are used to transfer certain
     * kernel level information to the user processes.
     *
     * ## Example
     *
     * ```
     * sp -= 8; *(size_t *)sp = AT_NULL;
     * sp -= 8; *(size_t *)sp = PGSIZE;
     * sp -= 8; *(size_t *)sp = AT_PAGESZ;
     *
     * sp -= 8; *(size_t *)sp = 0;
     *
     * // envp here. Ignore it if your don't want to implement envp.
     *
     * sp -= 8; *(size_t *)sp = 0;
     *
     * // argv here.
     *
     * sp -= 8; *(size_t *)sp = argc;
     *
     * // Stack pointer must be aligned to 16B!
     *
     * thisproc()->tf->sp = sp;
     * ```
     *
     */
}
