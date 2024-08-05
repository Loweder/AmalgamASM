  .code16

  .globl main
  .equ abs, 0x20
  nop
  nop
main:
  nop
  mov $abs, %r2
  jmp $main+2
