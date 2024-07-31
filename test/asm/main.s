;  .code16
;  
;  .int 0xCA1836AC
;
;  .org 0x100
;main:
;  mov %r1, 0x1000
;  mov %r2, 0xCAFE
;  mov %r3, 0x2000
;  mov [%r3], 0x3040
;  mov %r4, [%r3]
;  push %r2
;  pop %r5
;  swap %r3, %r5
;  mov %er6 [%er1]
;  .org 0x1000

.macro printf mess
  mov $\mess, %r1
  call $0x4050
.endm

.macro simple_m
  call $0x1516
  xor %r1,%r1
.endm

.macro clx_m al,bt=5
  add $0xAB\al, (%r\bt)
lbl\al:
  jmp \al, \bt
.endm

.macro empty a,b
  mov \a,\b
.endm

.set ALPHA 10
.set BETA 52
.set R1 1
.set R4 2
.equiv GAMMA 78

.irpc val, ABCD
  printf "Oh, thats   \val"
.endr

.ifdef ALPHA
  .ifdef PROTO
    printf "PROTO and ALPHA set"
  .else
    printf "not PROTO but ALPHA set"
  .endif
.else
  .ifndef BETA
    printf "Won't see this"
  .else
    printf "Should not see this"
  .endif
  .ifdef GAMMA
    printf "And this"
  .endif
.endif

.irpc val, 12345
  .ifdef R\val
    printf "Cool"
  .else
    printf "Chill"
  .endif
  sub %r3, %r2
  mov $0x\val, %r8
  mul %r8, %r2
.endr

.irp data, alpha,beta,gamma,yota,zeta
  mov $\data, %r7
.endr


