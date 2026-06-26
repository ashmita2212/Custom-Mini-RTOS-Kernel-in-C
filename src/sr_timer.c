/*
 * sr_timer.c
 *
 * RTOS Software Timers Management
 */

#include "sr_kernel.h"
#include <stdlib.h>

/* Internal Timeout Tracking Structure (matches sr_delay.c) */
typedef struct sr_timeout {
  sTaskHandle_t     *task;  // Set to NULL for timers
  sTimerHandle_t    *timer;
  sUBaseType_t      dontRunUntil; // Time in ticks when the timer can run
  struct sr_timeout *next;
} sr_timeout_t;

/* Internal Scheduler & Handler Linkages */
extern void sr_insertTimeout(sr_timeout_t *delay);
extern void sr_removeTimerTimeoutList(sTimerHandle_t *timer);
extern volatile sUBaseType_t sr_isTimerRunning;

/*
 * Low-Level Timer Execution Assembly 
 */

__STATIC_FORCEINLINE__ void sr_timerReturn(void *arg)
{
  (void)arg;
  sr_isTimerRunning = 0;
  __asm volatile(
      "cpsie  i\n"
      "svc    #1");
}

__STATIC_NAKED__ void sr_timerStart(sTimerHandle_t *timer, sTimerFunc_t timerTask)
{
  (void)timer;
  (void)timerTask;
  __asm volatile(
      "sub  sp, #32 \n" // Keep the register that restores the context unchanged and reusable
      "bx   r1      \n" // timerTask is the second argument, thus stored in r1
      ::: "memory");
}

/*
 * Internal Timer Management
 */

void sr_insertTimer(sTimerHandle_t *timerHandle)
{
  sUBaseType_t temp = sGetTick();
  sr_timeout_t *delay = (sr_timeout_t *)malloc(sizeof(sr_timeout_t));
  
  if (delay == NULL) return;

  delay->task = NULL;
  delay->timer = timerHandle;
  delay->dontRunUntil = SR_SAT_ADD_U32(temp, timerHandle->Period);
  delay->next = NULL;

  // sr_insertTimeout handles its own critical regions
  sr_insertTimeout(delay);
}

static sUBaseType_t *sr_taskInitStackTimer(sTimerFunc_t timerFunc, sTimerHandle_t *arg)
{
  sUBaseType_t stacksize = SR_CONTEXT_STACK_SIZE + SR_TIMER_TASK_STACK_WORDS;
  sUBaseType_t *stack = (sUBaseType_t *)malloc(sizeof(sUBaseType_t) * stacksize);
  
  if (stack == NULL)
    return NULL;

  /*
  Hardware automatically pushes these registers onto the stack (in this order):
      r0, r1, r2, r3, r12, lr (return address), pc (program counter), xPSR
  */

  stack[stacksize - 8] = (sUBaseType_t)arg;            // R0
  stack[stacksize - 7] = (sUBaseType_t)(timerFunc);    // R1
  stack[stacksize - 3] = (sUBaseType_t)(sr_timerReturn); // LR
  stack[stacksize - 2] = (sUBaseType_t)(sr_timerStart);  // PC
  stack[stacksize - 1] = 0x01000000;                   // xPSR (Thumb mode)

#ifdef DEBUG
  stack[stacksize - 6] = 0x22222223;  // R2
  stack[stacksize - 5] = 0x33333334;  // R3
  stack[stacksize - 4] = 0xCCCCCCCE;  // R12
  stack[stacksize - 9] = 0x77777778;  // r7
  stack[stacksize - 10] = 0x66666667; // r6
  stack[stacksize - 11] = 0x55555556; // r5
  stack[stacksize - 12] = 0x44444445; // r4
  stack[stacksize - 13] = 0xBBBBBBBC; // r11
  stack[stacksize - 14] = 0xAAAAAAAB; // r10
  stack[stacksize - 15] = 0x9999999A; // r9
  stack[stacksize - 16] = 0x88888889; // r8
#endif

  return stack;
}

/*
 * Public API
 */

sRTOS_StatusTypeDef sRTOSTimerCreate(
    sTimerFunc_t timerTask,
    sUBaseType_t id,
    sBaseType_t period,
    sUBaseType_t autoReload,
    sTimerHandle_t *timerHandle)
{
  // 7 words added to save r4-r11
  sUBaseType_t *stack = sr_taskInitStackTimer(timerTask, timerHandle);
  if (stack == NULL)
    return sRTOS_ALLOCATION_FAILED;

  timerHandle->stackPt = (sUBaseType_t *)(stack + SR_TIMER_TASK_STACK_WORDS); 
  timerHandle->stackBase = (sUBaseType_t *)stack;
  timerHandle->id = id;
  timerHandle->Period = period;
  timerHandle->autoReload = (sbool_t)autoReload;
  timerHandle->status = sReady;

  sr_insertTimer(timerHandle);
  return sRTOS_OK;
}

// Stopping while the timer is still running will not stop it immediately.
// Stop will prevent the timer from running again until resumed.
void sRTOSTimerStop(sTimerHandle_t *timerHandle)
{
  if (timerHandle->status == sReady)
  {
    __sCriticalRegionBegin();
    timerHandle->status = sBlocked;
    __sCriticalRegionEnd(); // BUGFIX: Missing unlock in original code
    
    // List removal handles its own internal locking
    sr_removeTimerTimeoutList(timerHandle); 
  }
}

void sRTOSTimerResume(sTimerHandle_t *timerHandle)
{
  if (timerHandle->status == sBlocked)
  {
    __sCriticalRegionBegin();
    timerHandle->status = sReady;
    __sCriticalRegionEnd(); // BUGFIX: Missing unlock in original code
    
    sr_insertTimer(timerHandle);
  }
}

// If a NULL or invalid timerHandle is provided, no action is taken.
// Deleting while the timer is still running causes a use-after-free.
// Do NOT delete a timer until the timer is no longer in use.
void sRTOSTimerDelete(sTimerHandle_t *timerHandle)
{
  if (timerHandle == NULL) return;
  
  sr_removeTimerTimeoutList(timerHandle);
  free(timerHandle->stackBase);
}

void sRTOSTimerUpdatePeriod(sTimerHandle_t *timerHandle, sBaseType_t period)
{
  __sCriticalRegionBegin();
  timerHandle->Period = period;
  __sCriticalRegionEnd(); // BUGFIX: Missing unlock in original code
  
  sr_removeTimerTimeoutList(timerHandle);
  sr_insertTimer(timerHandle);
}
