inc:
    sub     sp, sp, #0x20
    adrp    x8, 0x80010000
    mov     w11, #0x1
    add     x12, sp, #0x8
    ldr     x8, [x8, #4040]
    mrs     x9, tpidr_el0
    ldr     x10, [x9, x8]
    str     x11, [sp, #16]
    str     x10, [sp, #8]
    str     x12, [x9, x8]
    str     x0, [sp, #24]
    tbz     x0, #63, merge
    ubfx    x11, x0, #32, #31
    cbz     x11, merge
    ldr     x11, [x11]
    add     x0, x11, w0, uxtw
merge:
    ldr     w11, [x0]
    add     w11, w11, #0x1
    str     w11, [x0]
    str     xzr, [sp, #24]
    str     x10, [x9, x8]
    add     sp, sp, #0x20
