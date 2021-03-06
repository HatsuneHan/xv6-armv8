/* Context switch
 * 
 *   void swtch(struct context **old, struct context *new);
 * 
 * Save current register context on the stack, 
 * creating a struct context, and save its address in *old.
 * Switch stacks to new and pop previously-saved registers.
 */
.global swtch

swtch:
    /* TODO: Your code here. */
    stp	x29, x30, [sp, #-16]!
    stp	x27, x28, [sp, #-16]!
    stp	x25, x26, [sp, #-16]!
    stp	x23, x24, [sp, #-16]!
    stp	x21, x22, [sp, #-16]!
    stp	x19, x20, [sp, #-16]!
    stp	x17, x18, [sp, #-16]!
    str	x16, [sp, #-8]!

    mov x21, sp
    str x21, [x0]
    mov sp, x1

    ldr	x16, [sp], #8
    ldp	x17, x18, [sp], #16
    ldp	x19, x20, [sp], #16
    ldp	x21, x22, [sp], #16
    ldp	x23, x24, [sp], #16
    ldp	x25, x26, [sp], #16
    ldp	x27, x28, [sp], #16
    ldp	x29, x30, [sp], #16

    br x30
