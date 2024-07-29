; Include external file
.include alt.s

; Define some constants and labels
.equ MAX_COUNT, 10
.equ DEBUG, 1
START: 
    mov eax, 0 ; Initialize eax

; Macro definition
.macro PRINT_MSG msg
    ; Print message (pseudo code)
    call print, \msg
.endm

; Macro usage
PRINT_MSG "Starting the loop"

; Loop through numbers 1 to MAX_COUNT
LOOP_START:
    .irp num, 1,2,3,4,5
        cmp eax, \num
        je LABEL_\num
    .endr
    jmp END

LOOP_END:
    ; Conditional compilation
    .ifdef DEBUG
        PRINT_MSG "Debug mode active"
    .endif

    ; Nested conditionals
    .ifndef RELEASE
        PRINT_MSG "Not in release mode"
        .ifdef DEBUG
            PRINT_MSG "Also in debug mode"
        .else
            PRINT_MSG "Not in debug mode"
        .endif
    .else
        PRINT_MSG "In release mode"
    .endif

; Test of .irpc
TEST_IRPC:
    .irpc char, ABCD
        ; Pseudo instruction using char
        call print_char, \char
    .endr

END:
    mov eax, 0 ; End program
