#include <stdint.h>

#include "arm.h"
#include "string.h"
#include "console.h"
#include "kalloc.h"
#include "memlayout.h"
#include "vm.h"
#include "mmu.h"

#include "trap.h"
#include "timer.h"
#include "spinlock.h"
#include "proc.h"
#include "sd.h"
#include "log.h"

struct cpu cpus[NCPU];

// from tyf
struct function_lock {
    int count;
    struct spinlock lock;
};
struct function_lock init_kernel = { 0 };
struct function_lock memset_lock = { 0 };
struct function_lock initproc_once = { 0 };
struct function_lock sd_lock = { 0 };
struct function_lock fs_lock = { 0 };


void
main()
{
    /*
     * Before doing anything else, we need to ensure that all
     * static/global variables start out zero.
     */

    extern char edata[], end[], vectors[];

    /*
     * Determine which functions in main can only be
     * called once, and use lock to guarantee this.
     */
     /* TODO: Your code here. */


    /* TODO: Use `memset` to clear the BSS section of our program. */
    acquire(&memset_lock.lock);
    if (!memset_lock.count) {
        memset_lock.count = 1;
        memset(edata, 0, end - edata);
    }
    release(&memset_lock.lock);

    console_init();

    acquire(&init_kernel.lock);
    if (!init_kernel.count) {
        cprintf("main: [CPU%d] is init kernel\n", cpuid());

        init_kernel.count = 1;
        alloc_init();
        cprintf("Allocator: Init success.\n");
        check_free_list();

    }
    release(&init_kernel.lock);
    irq_init();
    acquire(&initproc_once.lock);
    if (!initproc_once.count) {
        initproc_once.count = 1;
        proc_init();
        user_init();
        user_idle_init();
        user_idle_init();
        user_idle_init();
        user_idle_init();
        sd_init();
        binit();
        fileinit();

        cprintf("init the proc successfully\n");
    }
    release(&initproc_once.lock);

    lvbar(vectors);
    timer_init();

    cprintf("main: [CPU%d] Init success.\n", cpuid());
    scheduler();


    while (1);
}
