.section .text
.global _start
_start:
    la sp, _stack_top     # initialize stack
    call main             # call main()
1:  j 1b                  # infinite loop after main returns

.section .bss
_stack_bottom:
    .space 1024           # 1KB stack
_stack_top: