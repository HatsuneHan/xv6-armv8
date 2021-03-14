/* File descriptors */

#include "types.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "console.h"
#include "log.h"

struct devsw devsw[NDEV];
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

/* Optional since BSS is zero-initialized. */
void
fileinit()
{
    /* TODO: Your code here. */
    initlock(&ftable.lock, "ftable");
}

/* Allocate a file structure. */
struct file*
    filealloc()
{
    /* TODO: Your code here. */
    struct file* f;
    acquire(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if (f->ref == 0) {
            f->ref = 1;
            release(&ftable.lock);
            return f;
        }
    }

    release(&ftable.lock);
    return 0;
}

/* Increment ref count for file f. */
struct file*
    filedup(struct file* f)
{
    /* TODO: Your code here. */
    acquire(&ftable.lock);

    if (f->ref <= 0) {
        panic("filedup");
    }

    f->ref++;

    release(&ftable.lock);
    return f;
}

/* Close file f. (Decrement ref count, close when reaches 0.) */
void
fileclose(struct file* f)
{
    /* TODO: Your code here. */
    struct inode* f_inode;
    acquire(&ftable.lock);
    if (f->ref < 1) {
        panic("fileclose: ref %d\n", f->ref);
    }

    if (--f->ref > 0) {
        release(&ftable.lock);
        return;
    }



    f_inode = f->ip;
    if (f->type == FD_INODE) {
        begin_op();
        iput(f_inode);
        end_op();
    }
    else {
        panic("fileclose: unsupported file type\n");
    }

    f->type = FD_NONE;

    release(&ftable.lock);
}

/* Get metadata about file f. */
int
filestat(struct file* f, struct stat* st)
{
    /* TODO: Your code here. */
    if (f->type == FD_INODE) {
        ilock(f->ip);
        stati(f->ip, st);
        iunlock(f->ip);
        return 0;
    }
    return -1;
}

/* Read from file f. */
ssize_t
fileread(struct file* f, char* addr, ssize_t n)
{
    /* TODO: Your code here. */
    ssize_t r;
    if (f->readable == 0) {
        return -1;
    }
    switch (f->type) {

    case FD_INODE:
        ilock(f->ip);
        r = readi(f->ip, addr, f->off, n);
        if (r > 0) {
            f->off += r;
        }
        iunlock(f->ip);
        return r;
    default:
        panic("fileread");
    }
    return 0;
}

/* Write to file f. */
ssize_t
filewrite(struct file* f, char* addr, ssize_t n)
{
    /* TODO: Your code here. */
    // acquire(&ftable.lock);
    ssize_t n1;
    ssize_t r;
    int max;
    if (!f->writable) {
        return -1;
    }

    switch (f->type) {

    case FD_INODE:
        max = ((LOGSIZE - 4) >> 1) * 512;
        int i;
        for (i = 0; i < n;) {
            r = MIN(max, n - i);
            begin_op();
            ilock(f->ip);

            n1 = writei(f->ip, addr + i, f->off, r);
            if (n1 > 0) {
                f->off += n1;
            }

            iunlock(f->ip);
            end_op();

            if (n1 < 0) {
                panic("filewrite: n1 < 0 \n");
            }

            if (n1 < r) {
                panic("filewrite: n1 < r");
            }
            i += r;
        }
        return i == n ? n : -1;
        break;
    default:
        panic("filewrite: no type");
    }
    return -1;
}


