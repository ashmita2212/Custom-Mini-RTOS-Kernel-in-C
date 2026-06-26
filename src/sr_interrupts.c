/*
 * sr_interrupts.c
 *
 * RTOS System Interrupt Handlers (SysTick, SVC)
 */

#include "sr_kernel.h"

/* Global System State Variables */
volatile sUBaseType_t sr_tickCount = 0;
volatile sUBaseType_t sr_isTimerRunning = 0;

/* External references to core kernel routines (implemented in scheduler/timer) */
extern void sr_Scheduler_Handler(void);
extern void sr_TimerReturn_Handler(void);

/**
 * @brief Safely retrieve the current system tick count.
 * @return The current number of ticks since the scheduler started.
 */
sUBaseType_t sGetTick(void)
{
  sUBaseType_t temp;
  
  /* Enter critical section to ensure the 32-bit read is atomic */
  __sCriticalRegionBegin();
  temp = sr_tickCount;
  __sCriticalRegionEnd();
  
  return temp;
}

/* * Weakly defined CMSIS interrupt handlers.
 * These act as default placeholders and are typically overridden 
 * by specific hardware port assembly files (e.g., sr_port_cm4.s) 
 * or internal RTOS routines to perform actual context switching.
 */
__attribute__((weak)) void SysTick_Handler(void) 
{
    /* Default empty handler */
}

__attribute__((weak)) void SVC_Handler(void) 
{
    /* Default empty handler */
}