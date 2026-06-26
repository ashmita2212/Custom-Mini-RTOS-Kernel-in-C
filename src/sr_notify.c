/*
 * sr_notify.c
 *
 * RTOS Direct-to-Task Notifications
 */

#include "sr_kernel.h"
#include <stdlib.h>

/* Internal Scheduler Linkages */
extern void sr_deleteTask(sTaskHandle_t *task, sbool_t freeMem);
extern void sr_insertTask(sTaskHandle_t *task);
extern sTaskHandle_t *sr_currentTask;

/*
 * Internal function to handle priority inheritance and message delivery.
 * Refactored to accept a pointer to allow proper NULL checking when 
 * mutexes trigger priority updates without sending a distinct message.
 */
void sr_pushTaskNotification(sTaskHandle_t *task, sUBaseType_t *messagePtr, sPriority_t priority)
{
  __sCriticalRegionBegin();
  
  if (messagePtr != NULL)
  {
    task->hasNotification = sTrue;
    task->notificationMessage = *messagePtr;
  }
  
  // Handle priority inheritance
  if (task->priority < priority)
  {
    task->priority = priority;
    sr_deleteTask(task, sFalse);
    sr_insertTask(task);
  }
  
  __sCriticalRegionEnd();
}

/*
 * Public API
 */

void sRTOSTaskNotify(sTaskHandle_t *taskToNotify, sUBaseType_t message)
{
  sr_pushTaskNotification(taskToNotify, &message, sr_currentTask->priority);
}

void sRTOSTaskNotifyFromISR(sTaskHandle_t *taskToNotify, sUBaseType_t message)
{
  sr_pushTaskNotification(taskToNotify, &message, sPriorityMax);
}

sUBaseType_t sRTOSTaskNotifyTake(sUBaseType_t timeoutTicks)
{
  sUBaseType_t timeoutFinish = SR_SAT_ADD_U32(sGetTick(), timeoutTicks);
  
  __sCriticalRegionBegin();
  while (!sr_currentTask->hasNotification)
  {
    __sCriticalRegionEnd();
    if (timeoutFinish <= sGetTick())
    {
      return 0; // Return 0 (no message) on timeout
    }
    sRTOSTaskYield();
    __sCriticalRegionBegin();
  }
  
  sr_currentTask->hasNotification = sFalse;
  __sCriticalRegionEnd();
  
  return sr_currentTask->notificationMessage;
}