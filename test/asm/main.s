  .code16
  
  .int 0xCA1836AC

  .org 0x100
main:
  mov %r1, 0x1000
  mov %r2, 0xCAFE
  mov %r3, 0x2000
  mov [%r3], 0x3040
  mov %r4, [%r3]
  push %r2
  pop %r5
  swap %r3, %r5
  mov %er6 [%er1]
  .org 0x1000
