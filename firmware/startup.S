.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.word _stack_top
.word Reset_Handler

.section .text.Reset_Handler, "ax", %progbits
.global Reset_Handler
.thumb_func
Reset_Handler:
    bl main
1:
    b 1b
