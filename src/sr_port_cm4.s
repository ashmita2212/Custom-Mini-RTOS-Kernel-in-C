.syntax unified
.cpu cortex-m4
.thumb

/* External references to C variables and functions */
.extern sr_tickCount
.extern sr_currentTask
.extern sr_getFirstAvailableTask
.extern sr_checkExpiredTimeout
.extern sr_isTimerRunning
.extern sr_ticksExecutingCurrentTask

.global SysTick_Handler
.global SVC_Handler
.global sr_Scheduler_Handler
.global sRTOSStartScheduler
.global sr_TimerReturn_Handler

#include "sr_config.h"

.section .text.SysTick_Handler,"ax",%progbits
.type SysTick_Handler, %function
SysTick_Handler:
    cpsid   i                           // disable isr
    
    ldr     r0, =sr_tickCount
    ldr     r1, [r0]                    // read sr_tickCount
    adds    r1, #1
    str     r1, [r0]                    // save sr_tickCount++

    ldr     r0, =sr_ticksExecutingCurrentTask
    ldr     r1, [r0]                    // read sr_ticksExecutingCurrentTask
    adds    r1, #1
    str     r1, [r0]                    // save sr_ticksExecutingCurrentTask++

    ldr     r0, =sr_isTimerRunning      
    ldr     r0, [r0]                    // read value of sr_isTimerRunning
    cmp     r0, #1
    beq     2f                          // if a timer is running 
    
Timer_return:
    push    {lr}
    bl      sr_checkExpiredTimeout      // get timer available else return null (also decrements timers)
    pop     {lr}
    
    cmp     r0, #0                      // check if NULL
    bne     sr_Timer_Handler            // branch to timer handler if a timer is ready

#if SR_USE_PREEMPTION == 1
    b       sr_Scheduler_Handler        // run scheduler if preemption is enabled
#else
    cpsie   i
    bx      lr
#endif
2:
    cpsie   i
    bx      lr
.size SysTick_Handler, .-SysTick_Handler



.section .text.SVC_Handler,"ax",%progbits
.type SVC_Handler, %function
SVC_Handler:
    cpsid   i                           // disable isr
    TST     lr, #4                     
    ITE     EQ                         
    MRSEQ   r0, MSP                  
    MRSNE   r0, PSP                  
    mov     r1, r0                 
    adds    r1, #24                     // uint8_t *pc = (uint8_t *)sp[6]; (stacked PC)
    ldr     r1, [r1, #0]          
    ldrb.w  r1, [r1, #-2]               // uint8_t svc_number = pc[-2];
    
    cmp     r1, #0                      // Yield (SVC #0)
    beq     sr_Scheduler_Handler_andReset   
    
    cmp     r1, #1                 
    beq     sr_TimerReturn_Handler      // Timer Return (SVC #1)
    
    cpsie   i                    
    bx      lr                   

sr_Scheduler_Handler_andReset:
#if SR_USE_PREEMPTION == 1
    ldr     r1, =sr_ticksExecutingCurrentTask
    mov     r2, #SR_TIME_SLICE_TICKS
    str     r2, [r1]                    // reset sr_ticksExecutingCurrentTask
#endif
    b       sr_Scheduler_Handler
.size SVC_Handler, .-SVC_Handler



/*
When an interrupt occurs on ARM Cortex-M:

1. **Hardware automatically saves context:** The processor pushes these registers onto the current stack (MSP or PSP):
   - `r0`, `r1`, `r2`, `r3`, `r12`, `lr`, `pc`, `xPSR`

2. **Software can save more context:** If needed, the interrupt handler (or RTOS) can manually push additional 
   registers (`r4`–`r11`, etc.) to the stack.

3. **On return:** The processor pops the saved registers from the stack, restoring the previous 
   state and resuming execution.
*/

.section .text.sr_TaskSaveRegisters,"ax",%progbits
.type sr_TaskSaveRegisters, %function      
sr_TaskSaveRegisters:
// argument r0: TaskHandle for the task you want to save registers
// note: r1 is changed in this routine
// use register to save the lr before jumping to this routine because the stack will change
    ldrb    r1, [r0, #11]               // read current registersSaved
    cmp     r1, #1                      // check if the task has saved registers
    beq     1f
    
    push    {r4-r7}                     // save r4,r5,r6,r7
    stmdb   sp!, {r8-r11}               // save r8,r9,r10,r11
    
    ldrb    r1, [r0, #8]                // Task->fps
    cmp     r1, #1                      // check if floating point stage is on
    bne     1f                          
    
    vstmdb  sp!, {s0-s31}               // if float point mode is on, save fpu registers
    vmrs    r1, fpscr                   
    push    {r1}                        
1:
    mov     r1, #1
    strb    r1, [r0, #11]               // change registersSaved to true
    str     sp, [r0]                    // save the new current task sp
    bx      lr
.size sr_TaskSaveRegisters, .-sr_TaskSaveRegisters

.section .text.sr_TaskRestoreRegisters,"ax",%progbits
.type sr_TaskRestoreRegisters, %function
sr_TaskRestoreRegisters:
// argument r0: TaskHandle for the task you want to restore registers
// note: r1 is changed in this routine
    ldr     sp, [r0]                    // set task sp
    ldrb    r1, [r0, #11]               // read current registersSaved
    cmp     r1, #1                      // check if the task has saved registers
    bne     1f
    
    ldrb    r1, [r0, #8]                // Task->fps
    cmp     r1, #1                      
    bne     1f                          
    
    pop     {r1}                        // if float point mode is on, restore fpu registers
    vmsr    fpscr, r1                   
    vldmia  sp!, {s0-s31}               
1:
    ldmia   sp!, {r8-r11}               
    pop     {r4-r7}                     
    
    mov     r1, #0
    strb    r1, [r0, #11]               // change registersSaved to false
    bx      lr
.size sr_TaskRestoreRegisters, .-sr_TaskRestoreRegisters


.section .text.sr_Scheduler_Handler,"ax",%progbits
.type sr_Scheduler_Handler, %function
sr_Scheduler_Handler:                   // r0,r1,r2,r3,r12,lr,pc,psr saved by interrupt
    push    {lr}                        // save return address
    bl      sr_getFirstAvailableTask    // returns the highest priority ready task
    pop     {lr}                        // restore return address
    
    cmp     r0, #0                      // if AvailableTask is null then keep executing current task
    beq     2f                          
    
    mov     r2, r0
    ldr     r0, =sr_currentTask         // get the current task pointer
    ldr     r0, [r0]                    // load the address of current task
    
    mov     r3, lr
    bl      sr_TaskSaveRegisters
    mov     lr, r3
    
    ldrb    r3, [r0, #9]                // read current status
    cmp     r3, #1                      // check if status is sRunning
    bne     1f
    mov     r3, #2                      // sReady: 0x02
    strb    r3, [r0, #9]                // change current task status to sReady
1:
    mov     r0, r2                      // copy firstAvailableTask addr into r0
    mov     r3, lr
    bl      sr_TaskRestoreRegisters
    mov     lr, r3
    
    ldr     r1, =0x1                    // sRunning: 0x1
    strb    r1, [r0, #9]                // change next task status to sRunning

    ldr     r1, =sr_currentTask
    str     r0, [r1]                    // change the current running task ptr
2:
    cpsie   i                           // enable isr  
    bx      lr                          // return and start the next task
.size sr_Scheduler_Handler, .-sr_Scheduler_Handler


.section .text.sr_Timer_Handler,"ax",%progbits
.type sr_Timer_Handler, %function
sr_Timer_Handler:                       // r0,r1,r2,r3,r12,lr,pc,psr saved by interrupt
    ldr     r2, =sr_isTimerRunning
    mov     r1, #1                      
    str     r1, [r2]                    // change the sr_isTimerRunning to true
    mov     r2, r0                      // save a copy of r0 (timerHandle) into r2
    
    ldr     r0, =sr_currentTask         
    ldr     r0, [r0]

    mov     r3, lr
    bl      sr_TaskSaveRegisters
    mov     lr, r3

    mov     r0, r2                      // restore the timerHandle to r0 (argument for the timer)
    ldr     sp, [r0]                    // SP = Timer Task stack
    
    cpsie   i                           // isr is enabled
    bx      lr                          // return and start the timer task
.size sr_Timer_Handler, .-sr_Timer_Handler



.section .text.sr_TimerReturn_Handler,"ax",%progbits
.type sr_TimerReturn_Handler, %function
sr_TimerReturn_Handler:
    cpsid   i                           // disable isr
    ldr     r0, =sr_currentTask         
    ldr     r0, [r0]
    mov     r3, lr
    bl      sr_TaskRestoreRegisters
    mov     lr, r3
    b       Timer_return                // rerun the scheduler in case the timer ran at the end of quantum
.size sr_TimerReturn_Handler, .-sr_TimerReturn_Handler


.section .text.sRTOSStartScheduler,"ax",%progbits
.type sRTOSStartScheduler, %function
sRTOSStartScheduler:
    cpsid   i                           // disable isr
    push    {lr}                        // save return address
    bl      sr_getFirstAvailableTask    // returns the first highest ready task
    pop     {lr}                        // restore return address
    
    ldr     sp, [r0]                    // switch to the sp of the task
    ldrb    r1, [r0, #8]                // r1 = task->fps

    cmp     r1, #1                      // check if task->fps is on
    ite     eq
    addeq   sp, sp, #166                // bytes used to restore registers if floating point is ON
    addne   sp, sp, #32                 // bytes used to restore registers if floating point is OFF
    add     sp, sp, #24 
    
    pop     {r1}                        // pop the address of the task to execute
    mov     lr, r1                      // set return address to task entry point so it returns to task instead of main
    
    ldr     r1, =0x1                    // sRunning: 0x1
    strb    r1, [r0, #9]                // change task status to sRunning
    
    ldr     r2, =sr_currentTask         // get the address of sr_currentTask
    str     r0, [r2]                    // save the current task ptr
    
    mov     r1, #0
    strb    r1, [r0, #11]               // change registersSaved to false (because registers are restored)
    
    // Enable SysTick
    ldr     r0, =0xE000E010             // SYST_CSR
    movs    r1, #7                      // ENABLE | TICKINT | CLKSOURCE
    str     r1, [r0]                    // enable SysTick, enable exception, select processor clock
    
    cpsie   i                           // enable irq
    bx      lr                          // return (jumps to the first task)
.size sRTOSStartScheduler, .-sRTOSStartScheduler