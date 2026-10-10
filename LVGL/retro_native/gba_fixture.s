.syntax unified
.arm
.global _start
_start:
    b entry
.org 0xc0
entry:
    mov r0, #0x04000000
    ldr r1, =0x0403
    strh r1, [r0]
    mov r0, #0x06000000
    mov r1, #0x1f
    ldr r2, =38400
fill:
    strh r1, [r0], #2
    subs r2, r2, #1
    bne fill
forever:
    b forever
.ltorg
.org 65535
.byte 0
