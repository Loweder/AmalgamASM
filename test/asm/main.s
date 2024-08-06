  .code16

  .comm com1_sym 8
  .lcomm com2_sym 4

  .globl main
  .equ abs, 0x20
  nop
  nop
  .equ sym_3 .*2
main:
  nop
  mov $abs, %r2
  jmp $main+2
  .=.+2
  .equ . .+2

  .data
