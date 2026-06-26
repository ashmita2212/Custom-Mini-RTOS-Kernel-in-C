/*
 * sr_task.c
 *
 * Task Creation, Deletion, and Context Initialization
 */

#include "sr_kernel.h"
#include <stdlib.h>
#include <string.h>

/* Internal Scheduler Linkages */
extern void sr_deleteTask(sTaskHandle_t *task, sbool_t freeMem);
extern void sr_insertTask(sTaskHandle_t *task);
extern void sr_removeTaskTimeoutList(sTaskHandle_t *task);
extern sTaskHandle_t *sr_currentTask;

// Added for stack overflow protection scheme
#define SR_STACK_MAGIC_WORD 0xDEADBEEF 

/* Catch-all function if a task illegally tries to return instead of looping/deleting itself */
__STATIC_NAKED__ void sr_taskReturn(void *arg)
{
  (void)arg;
  for (;;)
  {
    // A well-behaved task should never reach here
  }
}

static sUBaseType_t *sr_taskInitStack(sTaskFunc_t taskFunc, void *arg,
                                      sUBaseType_t stacksize, sUBaseType_t fpsMode)
{
  stacksize = ((fpsMode == SR_FALSE) ? SR_MIN_STACK_SIZE_NO_FPU : SR_MIN_STACK_SIZE_FPU) + stacksize;
  sUBaseType_t *stack = (sUBaseType_t *)malloc(sizeof(sUBaseType_t) * (stacksize));
  
  if (stack == NULL)
    return NULL;

  /* MODIFICATION: Pre-fill stack with a magic word to allow for deep boundary checking */
  for (sUBaseType_t i = 0; i < stacksize; i++) {
      stack[i] = SR_STACK_MAGIC_WORD;
  }

  /*
  Hardware automatically pushes these registers onto the stack (in this order):
      r0, r1, r2, r3, r12, lr (return address), pc (program counter), xPSR
  */

  stack[stacksize - 8] = (sUBaseType_t)arg;           // R0: First argument
  stack[stacksize - 3] = (sUBaseType_t)(sr_taskReturn); // LR: Return address 
  stack[stacksize - 2] = (sUBaseType_t)(taskFunc);    // PC: Entry point
  stack[stacksize - 1] = 0x01000000;                  // xPSR: Set to Thumb mode

#ifdef DEBUG
  // Software pushed registers (r4-r11) seeded for debugging visibility
  stack[stacksize - 7] = 0x11111112;  // R1
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
 * Note: Can be called after sRTOSStartScheduler() and the new task will be safely added.
 */
sRTOS_StatusTypeDef sRTOSTaskCreate(
    sTaskFunc_t taskFunc,
    char *name,
    void *arg,
    sUBaseType_t stacksizeWords,
    sPriority_t priority,
    sTaskHandle_t *taskHandle,
    sUBaseType_t fpsMode)
{
  sUBaseType_t *stack = sr_taskInitStack(taskFunc, arg, stacksizeWords, fpsMode);
  if (stack == NULL)
    return sRTOS_ALLOCATION_FAILED;

  taskHandle->stackBase = stack;
  
  /* Task Init */
  taskHandle->stackPt = (sUBaseType_t *)(stack + stacksizeWords); 
  taskHandle->nextTask = taskHandle;                              
  taskHandle->prevTask = taskHandle;
  taskHandle->status = sReady;
  taskHandle->registersSaved = sTrue; // Typo fixed: Registers initialized, meaning they are saved
  taskHandle->fps = (sbool_t)fpsMode;
  taskHandle->priority = priority;
  taskHandle->notificationMessage = 0;
  taskHandle->hasNotification = sFalse;
  taskHandle->originalPriority = priority;
  strncpy(taskHandle->name, name, SR_MAX_TASK_NAME_LEN);

  sr_insertTask(taskHandle);
  return sRTOS_OK;
}

/*
 * Implementation of the custom Stack Overflow check API
 */
sRTOS_StatusTypeDef sRTOSCheckStackOverflow(sTaskHandle_t *taskHandle)
{
  if (taskHandle == NULL)
      return sRTOS_ERROR; // Invalid handle
      
  // Check the deepest word of the allocated stack. If the magic word is gone, it overflowed!
  if (taskHandle->stackBase[0] != SR_STACK_MAGIC_WORD) {
      return sRTOS_ERROR; // Overflow detected
  }
  
  return sRTOS_OK; // Safe
}

void sRTOSTaskUpdatePriority(sTaskHandle_t *taskHandle, sPriority_t priority)
{
  __sCriticalRegionBegin();
  taskHandle->priority = priority;
  sr_deleteTask(taskHandle, sFalse);
  sr_insertTask(taskHandle);
  // sr_insertTask manages the critical region exit
}

void sRTOSTaskStop(sTaskHandle_t *taskHandle)
{
  if (taskHandle == NULL)
    taskHandle = sr_currentTask;

  if (taskHandle->status != sDeleted && taskHandle->status != sBlocked)
  {
    if (taskHandle->status == sWaiting)
    {
      sr_removeTaskTimeoutList(taskHandle);
    }
    else
    {
      // Removes the task from the ready list without freeing memory, allowing restoration
      sr_deleteTask(taskHandle, sFalse); 
    }

    __sCriticalRegionBegin();
    taskHandle->status = sBlocked;
    __sCriticalRegionEnd();

    if (taskHandle == sr_currentTask)
    {
      sRTOSTaskYield(); // Yield immediately if current task suspends itself
      return;
    }
  }
}

/*
 * Will yield if the priority of the current running task is lower than the resumed task
 */
void sRTOSTaskResume(sTaskHandle_t *taskHandle)
{
  if (taskHandle->status != sDeleted && taskHandle->status != sReady)
  {
    if (taskHandle->status == sWaiting)
    {
      sr_removeTaskTimeoutList(taskHandle);
    }
    
    __sCriticalRegionBegin();
    taskHandle->status = sReady;
    sr_insertTask(taskHandle); // Handles critical region end

    if (taskHandle->priority > sr_currentTask->priority)
    {
      sRTOSTaskYield();
      return;
    }
  }
}

// If provided a non-existing taskHandle, operates on the current task
void sRTOSTaskDelete(sTaskHandle_t *taskHandle)
{
  if (taskHandle == NULL)
    taskHandle = sr_currentTask;

  sr_deleteTask(taskHandle, sTrue);

  if (taskHandle == sr_currentTask)
  {
    sRTOSTaskYield(); // Self-deletion requires immediate yield
    return;
  }
}