/*
 * sr_queue.c
 *
 * RTOS Queue Inter-Process Communication (IPC)
 */

#include "sr_kernel.h"
#include <stdlib.h>
#include <string.h>

/* Masked global task tracker reference */
extern sTaskHandle_t *sr_currentTask;

void sRTOSQueueCreate(sQueueHandle_t *queueHandle, sUBaseType_t queueLength, sUBaseType_t itemSize)
{
  queueHandle->maxLength = queueLength; // Typo fixed
  queueHandle->length = 0;              // Typo fixed
  queueHandle->itemSize = itemSize;
  queueHandle->index = (sUBaseType_t)-1; 
  queueHandle->items = malloc(queueLength * sizeof(void *));
}

sbool_t sRTOSQueueReceive(sQueueHandle_t *queueHandle, void *itemPtr, sUBaseType_t timeoutTicks)
{
  sUBaseType_t timeoutFinish = SR_SAT_ADD_U32(sGetTick(), timeoutTicks);
  
  __sCriticalRegionBegin();
  while (queueHandle->length == 0)
  {
    __sCriticalRegionEnd();
    if (timeoutFinish <= sGetTick())
    {
      return sFalse;
    }
    sRTOSTaskYield();
    __sCriticalRegionBegin();
  }
  
  sUBaseType_t readPos = queueHandle->index % queueHandle->maxLength;
  void *temp = queueHandle->items[readPos];

  memcpy(itemPtr, temp, queueHandle->itemSize);
  free(temp);
  queueHandle->length--;
  
  __sCriticalRegionEnd();
  return sTrue;
}

sbool_t sRTOSQueueSend(sQueueHandle_t *queueHandle, void *itemPtr, sUBaseType_t timeoutTicks)
{
  sUBaseType_t timeoutFinish = SR_SAT_ADD_U32(sGetTick(), timeoutTicks);
  
  __sCriticalRegionBegin();
  while (queueHandle->length == queueHandle->maxLength)
  {
    __sCriticalRegionEnd();
    if (timeoutFinish <= sGetTick())
    {
      return sFalse;
    }
    sRTOSTaskYield();
    __sCriticalRegionBegin();
  }
  
  void *temp = malloc(queueHandle->itemSize);
  if (temp == NULL)
  {
    __sCriticalRegionEnd();
    return sFalse;
  }
  
  memcpy(temp, itemPtr, queueHandle->itemSize);
  queueHandle->index++;
  sUBaseType_t writePos = queueHandle->index % queueHandle->maxLength;
  queueHandle->items[writePos] = temp;
  queueHandle->length++;
  
  __sCriticalRegionEnd();
  return sTrue;
}

sbool_t sRTOSQueueSendFromISR(sQueueHandle_t *queueHandle, void *itemPtr)
{
  __sCriticalRegionBegin();
  if (queueHandle->length == queueHandle->maxLength)
  {
    __sCriticalRegionEnd();
    return sFalse;
  }
  
  void *temp = malloc(queueHandle->itemSize);
  if (temp == NULL)
  {
    __sCriticalRegionEnd();
    return sFalse;
  }
  
  memcpy(temp, itemPtr, queueHandle->itemSize);
  queueHandle->index++;
  sUBaseType_t writePos = queueHandle->index % queueHandle->maxLength;
  queueHandle->items[writePos] = temp;
  queueHandle->length++;
  
  __sCriticalRegionEnd();
  return sTrue;
}