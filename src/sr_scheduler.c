/*
 * sr_scheduler.c
 *
 * Core RTOS Scheduler & List Management
 */

#include "sr_kernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Cortex-M System Control Space (SCS) Registers */
#define SYST_CSR   (*((volatile uint32_t *)0xE000E010))
#define SYST_RVR   (*((volatile uint32_t *)0xE000E014))
#define SYST_CVR   (*((volatile uint32_t *)0xE000E018))
#define SYST_CALIB (*((volatile uint32_t *)0xE000E01C))
#define SYSPRI3    (*((volatile uint32_t *)0xE000ED20))

/* * Private RTOS Global Variables 
 */
// Each bit represents a priority; if set to 1, there are tasks to execute at that priority level
volatile sUBaseType_t sr_taskPriorityBitMap = 0x0; 

sTaskHandle_t *sr_taskList[SR_MAX_TASK_PRIORITY_COUNT] = {NULL};
sUBaseType_t sr_readyTaskCount[SR_MAX_TASK_PRIORITY_COUNT] = {0};
sTaskHandle_t *sr_idleTaskHandle;

// Set to the maximum time slice initially so the scheduler can begin immediately
volatile sUBaseType_t sr_ticksExecutingCurrentTask = SR_TIME_SLICE_TICKS; 

sTaskHandle_t *sr_currentTask;

/*
 * Internal Scheduler Functions
 */

void sr_idleTask(void *arg)
{
  (void)arg; // Unused parameter
  for (;;)
  {
    sRTOSTaskYield();
  }
}

void sr_readyTaskCounterInc(sPriority_t priority)
{
  // Offset by half to map negative priorities (-16 to 15) to positive array indices
  sUBaseType_t priorityIndex = priority + (SR_MAX_TASK_PRIORITY_COUNT / 2); 
  
  sr_readyTaskCount[priorityIndex]++;                      // Increment count for this priority
  sr_taskPriorityBitMap |= (1u << priorityIndex);          // Flag the bitmap to indicate a ready task
}

void sr_readyTaskCounterDec(sPriority_t priority)
{
  sUBaseType_t priorityIndex = priority + (SR_MAX_TASK_PRIORITY_COUNT / 2);

  sr_readyTaskCount[priorityIndex]--;
  if (sr_readyTaskCount[priorityIndex] == 0)
  {
    sr_taskPriorityBitMap &= ~(1u << priorityIndex); // Clear flag if no tasks remain at this priority
  }
}

// Tasks will always be inserted at the first position of their respective priority list.
void sr_insertTask(sTaskHandle_t *task)
{
  __sCriticalRegionBegin();
  
  sPriority_t priority = task->priority;
  sUBaseType_t priorityIndex = priority + (SR_MAX_TASK_PRIORITY_COUNT / 2); 
  sr_readyTaskCounterInc(priority);

  sTaskHandle_t *head = sr_taskList[priorityIndex];
  if (head == NULL)
  {
    task->nextTask = task;
    task->prevTask = task;
    sr_taskList[priorityIndex] = task;
    
    __sCriticalRegionEnd();
    return;
  }

  sTaskHandle_t *tail = head->prevTask;

  // Insert into circular doubly-linked list
  task->nextTask = head;
  task->prevTask = tail;
  tail->nextTask = task;
  head->prevTask = task;
  sr_taskList[priorityIndex] = task;

  __sCriticalRegionEnd();
}

void sr_deleteTask(sTaskHandle_t *task, sbool_t freeMem)
{
  __sCriticalRegionBegin();
  
  sPriority_t priority = task->priority;
  sUBaseType_t priorityIndex = priority + (SR_MAX_TASK_PRIORITY_COUNT / 2);
  sr_readyTaskCounterDec(priority);

  sTaskHandle_t *head = sr_taskList[priorityIndex];

  if (head != NULL)
  {
    if (head == task && task->nextTask == task) // Only element in the list
    {
      sr_taskList[priorityIndex] = NULL;
    }
    else
    {
      // Unlink from circular doubly-linked list
      sTaskHandle_t *prev = task->prevTask;
      sTaskHandle_t *next = task->nextTask;
      prev->nextTask = next;
      next->prevTask = prev;

      if (sr_taskList[priorityIndex] == task)
      {
        sr_taskList[priorityIndex] = next; // Move head if we removed the first element
      }
    }
  }

  // Clear links to avoid accidental dangling pointer usage
  task->nextTask = NULL;
  task->prevTask = NULL;

  __sCriticalRegionEnd();
  
  if (freeMem)
  {
    free(task->stackBase);
    free(task);
  }
}

/*
 * Public API
 */

sRTOS_StatusTypeDef sRTOSInit(sUBaseType_t BUS_FREQ)
{
  uint32_t PRESCALER = (BUS_FREQ / SR_TICK_RATE_HZ);

  if (PRESCALER == 0)
    return sRTOS_ERROR; // Avoid timer underflow

  SYST_CSR = 0;                             // Disable SysTick
  SYST_CVR = 0;                             // Clear current value
  SYST_RVR = (PRESCALER - 1) & 0x00FFFFFFu; // RVR is 24-bit max

  /* Set PendSV lowest priority, SysTick just above PendSV */
  uint8_t *shpr3 = (uint8_t *)&SYSPRI3;
  shpr3[2] = 0xF0; // PendSV priority byte
  shpr3[3] = 0xE0; // SysTick priority byte

  sr_idleTaskHandle = (sTaskHandle_t *)malloc(sizeof(sTaskHandle_t));
  if (sr_idleTaskHandle == NULL)
  {
    return sRTOS_ALLOCATION_FAILED;
  }
  
  sr_currentTask = sr_idleTaskHandle;
  
  return sRTOSTaskCreate(sr_idleTask,
                         "idle task",
                         NULL,
                         12,
                         sPriorityIdle,
                         sr_idleTaskHandle,
                         SR_FALSE);
}

sTaskHandle_t *sr_getFirstAvailableTask(void)
{
  sUBaseType_t currentPriorityIndex = sr_currentTask->priority + (SR_MAX_TASK_PRIORITY_COUNT / 2);
  sUBaseType_t priorityIndex; 
  
  // Find highest priority task using Count Leading Zeros hardware instruction
  unsigned int leadingZeros = __builtin_clz((unsigned int)sr_taskPriorityBitMap);
  priorityIndex = SR_MAX_TASK_PRIORITY_COUNT - (leadingZeros + 1);

  if (
#if SR_USE_PREEMPTION == 1
      sr_ticksExecutingCurrentTask >= SR_TIME_SLICE_TICKS // A time quantum has elapsed
      || priorityIndex > currentPriorityIndex             // A higher priority task is ready
#else
      1
#endif
  )
  {
    sr_ticksExecutingCurrentTask = 0; // Reset execution slice counter
    sTaskHandle_t *task = sr_taskList[priorityIndex];
    
    // Rotate tasks round-robin style
    sr_taskList[priorityIndex] = sr_taskList[priorityIndex]->nextTask; 

    if (task->priority != task->originalPriority)
    {
      // Priority inheritance or notification has modified the base priority
      task->priority = task->originalPriority;
      sr_deleteTask(task, sFalse);
      sr_insertTask(task);
    }
    return task;
  }

  return NULL; // Keep executing the current task
}