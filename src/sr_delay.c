/*
 * sr_delay.c
 *
 * RTOS Non-Blocking Task Delay and Timeout List Management
 */

#include "sr_kernel.h"
#include <stdlib.h>

/* Internal Scheduler Linkages */
extern void sr_readyTaskCounterDec(sPriority_t priority);
extern void sr_insertTask(sTaskHandle_t *task);
extern void sr_deleteTask(sTaskHandle_t *task, sbool_t freeMem);
extern sTaskHandle_t *sr_currentTask;

/* Timeout Tracking Structure */
typedef struct sr_timeout {
  sTaskHandle_t     *task;
  sTimerHandle_t    *timer;
  sUBaseType_t      dontRunUntil; // Time in ticks when the task/timer can run again
  struct sr_timeout *next;
} sr_timeout_t;

/* Global Timeout State */
volatile sUBaseType_t sr_earliestExpiringTimeout = 0;
sr_timeout_t *sr_timeoutList = NULL;

/*
 * Internal Timeout List Management
 */

void sr_insertTimeout(sr_timeout_t *timeout)
{
  __sCriticalRegionBegin();
  
  // Case 1: List is empty
  if (sr_timeoutList == NULL)
  {
    sr_timeoutList = timeout;
    sr_earliestExpiringTimeout = timeout->dontRunUntil;
    __sCriticalRegionEnd(); // BUGFIX: Original code incorrectly called Begin() here!
    return;
  }

  // Case 2: New timeout expires before the current earliest timeout
  if (timeout->dontRunUntil < sr_timeoutList->dontRunUntil)
  {
    sr_earliestExpiringTimeout = timeout->dontRunUntil;
    timeout->next = sr_timeoutList;
    sr_timeoutList = timeout;
    __sCriticalRegionEnd(); // BUGFIX
    return;
  }

  // Case 3: Insert in the middle or end of the list
  sr_timeout_t *curr = sr_timeoutList;
  while (curr->next && curr->next->dontRunUntil <= timeout->dontRunUntil)
  {
    curr = curr->next;
  }

  timeout->next = curr->next;
  curr->next = timeout;
  
  __sCriticalRegionEnd(); // BUGFIX
}

sr_timeout_t *sr_popFirstDelay(void)
{
  if (sr_timeoutList == NULL)
  {
    return NULL;
  }

  sr_timeout_t *first = sr_timeoutList;
  sr_timeoutList = first->next;

  if (sr_timeoutList != NULL) {
    sr_earliestExpiringTimeout = sr_timeoutList->dontRunUntil;
  } else {
    sr_earliestExpiringTimeout = 0; // List is now empty
  }
  
  return first;
}

void sr_removeTimerTimeoutList(sTimerHandle_t *timer)
{
  __sCriticalRegionBegin();
  if (sr_timeoutList == NULL)
  {
    __sCriticalRegionEnd();
    return;
  }

  if (sr_timeoutList->timer == timer)
  {
    sr_timeout_t *temp = sr_timeoutList;
    sr_timeoutList = sr_timeoutList->next;
    __sCriticalRegionEnd();
    free(temp);
    return;
  }

  sr_timeout_t *curr = sr_timeoutList;
  while (curr->next && curr->next->timer != timer)
  {
    curr = curr->next;
  }

  if (curr->next) {
    sr_timeout_t *temp = curr->next;
    curr->next = temp->next;
    __sCriticalRegionEnd();
    free(temp);
  } else {
    __sCriticalRegionEnd();
  }
}

void sr_removeTaskTimeoutList(sTaskHandle_t *task)
{
  __sCriticalRegionBegin();
  if (sr_timeoutList == NULL)
  {
    __sCriticalRegionEnd();
    return;
  }

  if (sr_timeoutList->task == task)
  {
    sr_timeout_t *temp = sr_timeoutList;
    sr_timeoutList = sr_timeoutList->next;
    __sCriticalRegionEnd();
    free(temp);
    return;
  }

  sr_timeout_t *curr = sr_timeoutList;
  while (curr->next && curr->next->task != task)
  {
    curr = curr->next;
  }

  if (curr->next) {
    sr_timeout_t *temp = curr->next;
    curr->next = temp->next;
    __sCriticalRegionEnd();
    free(temp);
  } else {
    __sCriticalRegionEnd();
  }
}

/*
 * Checks for tasks and timers that are done waiting.
 * Re-inserts tasks into the ready task list.
 * For timers, re-inserts into timeout list if autoReload is true,
 * then returns them to tell the scheduler a timer callback is ready.
 */
void *sr_checkExpiredTimeout(void)
{
  while (sr_timeoutList != NULL && sr_earliestExpiringTimeout <= sGetTick())
  {
    __sCriticalRegionBegin();
    sr_timeout_t *expiredTimeout = sr_popFirstDelay();
    
    if (expiredTimeout->task != NULL)
    {
      // sr_insertTask will exit the critical region internally
      sr_insertTask(expiredTimeout->task);
      free(expiredTimeout);
    }
    else if (expiredTimeout->timer != NULL)
    {
      if (expiredTimeout->timer->autoReload == sTrue)
      {
        expiredTimeout->dontRunUntil = SR_SAT_ADD_U32(expiredTimeout->dontRunUntil, expiredTimeout->timer->Period);
        // sr_insertTimeout handles critical region exit
        sr_insertTimeout(expiredTimeout); 
      }
      else
      {
        __sCriticalRegionEnd();
        free(expiredTimeout);
      }
      return expiredTimeout->timer; // Return timer handle for callback execution
    }
  }

  return NULL; 
}

/*
 * Public API
 */

void sRTOSTaskDelay(sUBaseType_t duration_ms)
{
  sUBaseType_t currentTick = sGetTick();
  sr_timeout_t *delay = (sr_timeout_t *)malloc(sizeof(sr_timeout_t));
  
  if (delay == NULL) return; // Guard against failed allocation

  delay->task = sr_currentTask;
  delay->timer = NULL;
  
  // Calculate expiry tick based on the configured kernel tick rate
  delay->dontRunUntil = SR_SAT_ADD_U32(currentTick, (duration_ms * (SR_TICK_RATE_HZ / 1000)));
  delay->next = NULL;

  __sCriticalRegionBegin();
  sr_currentTask->status = sWaiting;
  
  // Remove from ready list (without freeing memory)
  sr_deleteTask(sr_currentTask, sFalse); 
  
  // Insert into sleeping queue
  sr_insertTimeout(delay); 
  __sCriticalRegionEnd();
  
  sRTOSTaskYield(); // Yield CPU immediately to next task
}
