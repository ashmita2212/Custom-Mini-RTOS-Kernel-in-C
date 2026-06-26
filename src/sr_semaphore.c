/*
 * sr_semaphore.c
 *
 * RTOS Synchronization Primitives (Semaphores & Mutexes)
 */

#include "sr_kernel.h"
#include <stdlib.h>

/* Internal kernel dependencies */
extern void sr_pushTaskNotification(sTaskHandle_t *task, sUBaseType_t *message, sPriority_t priority);
extern sTaskHandle_t *sr_currentTask;

/*
 * Counting Semaphore API
 */

void sRTOSSemaphoreCreate(sSemaphore_t *sem, sBaseType_t n)
{
  *sem = n;
}

void sRTOSSemaphoreGive(sSemaphore_t *sem)
{
  __sCriticalRegionBegin();
  (*sem)++;
  __sCriticalRegionEnd();
}

sbool_t sRTOSSemaphoreTake(sSemaphore_t *sem, sUBaseType_t timeoutTicks)
{
  sUBaseType_t timeoutFinish = SR_SAT_ADD_U32(sGetTick(), timeoutTicks);
  
  __sCriticalRegionBegin();
  while (*sem <= 0)
  {
    __sCriticalRegionEnd();
    if (timeoutFinish <= sGetTick())
    {
      return sFalse;
    }
    __sCriticalRegionBegin();
  }

  (*sem)--;
  __sCriticalRegionEnd();
  return sTrue;
}

sbool_t sRTOSSemaphoreCooperativeTake(sSemaphore_t *sem, sUBaseType_t timeoutTicks)
{
  sUBaseType_t timeoutFinish = SR_SAT_ADD_U32(sGetTick(), timeoutTicks);
  
  __sCriticalRegionBegin();
  while (*sem <= 0)
  {
    __sCriticalRegionEnd();
    if (timeoutFinish <= sGetTick())
    {
      return sFalse;
    }
    sRTOSTaskYield(); // Yield control while waiting
    __sCriticalRegionBegin();
  }

  (*sem)--;
  __sCriticalRegionEnd();
  return sTrue;
}

/*
 * Mutex API
 */

void sRTOSMutexCreate(sMutex_t *mux)
{
  mux->sem = 1;
  mux->holderHandle = NULL;
}

sbool_t sRTOSMutexGive(sMutex_t *mux)
{
  // A mutex can only be given by its current owner
  if (mux->sem == 1 || mux->holderHandle != sr_currentTask)
    return sFalse;

  __sCriticalRegionBegin();
  sr_pushTaskNotification(mux->requesterHandle, NULL, sr_currentTask->priority);
  mux->sem++;
  __sCriticalRegionEnd();
  
  sRTOSTaskYield();
  return sTrue;
}

sbool_t sRTOSMutexGiveFromISR(sMutex_t *mux)
{
  if (mux->sem == 1)
    return sFalse;

  __sCriticalRegionBegin();
  // ISRs logically have a higher priority than any task, unblock requester immediately
  sr_pushTaskNotification(mux->requesterHandle, NULL, sPriorityMax); 
  mux->sem++;
  __sCriticalRegionEnd();
  
  return sTrue;
}

sbool_t sRTOSMutexTake(sMutex_t *mux, sUBaseType_t timeoutTicks)
{
  sUBaseType_t timeoutFinish = SR_SAT_ADD_U32(sGetTick(), timeoutTicks);
  
  __sCriticalRegionBegin();
  mux->requesterHandle = sr_currentTask; // Register intent to take
  
  while (mux->sem <= 0)
  {
    __sCriticalRegionEnd();
    if (timeoutFinish <= sGetTick())
    {
      return sFalse;
    }
    sRTOSTaskYield();
    __sCriticalRegionBegin();
  }

  mux->holderHandle = sr_currentTask;    // Claim ownership
  mux->sem--;
  __sCriticalRegionEnd();
  
  return sTrue;
}