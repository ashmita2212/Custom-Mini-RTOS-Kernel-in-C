/*
 * sr_types.h
 *
 * Core RTOS Data Types and Structures
 */

#ifndef SR_TYPES_H_
#define SR_TYPES_H_

#include <stddef.h>
#include <stdint.h>

/* Compiler directives */
#define __STATIC_FORCEINLINE__ __attribute__((always_inline)) static __inline
#define __STATIC_NAKED__       __attribute__((naked)) static

/* Core boolean definitions */
#define SR_FALSE 0u
#define SR_TRUE  1u

/* System memory definitions */
#define SR_CONTEXT_STACK_SIZE        8
#define SR_MIN_STACK_SIZE_NO_FPU     16
#define SR_MIN_STACK_SIZE_FPU        49
#define SR_MAX_TASK_NAME_LEN         12
#define SR_MAX_TASK_PRIORITY_COUNT   32

/* Math utility */
#define SR_SAT_ADD_U32(a, b) (((UINT32_MAX - (uint32_t)(a)) < (uint32_t)(b)) ? UINT32_MAX : (uint32_t)((uint32_t)(a) + (uint32_t)(b)))

/* Primitive Types */
typedef int32_t  sBaseType_t;
typedef uint32_t sUBaseType_t;

typedef enum {
  sFalse = 0u,
  sTrue
} sbool_t;

/* RTOS Status Codes */
typedef enum {
  sRTOS_OK = 0x00U,
  sRTOS_ERROR = 0x80U,
  sRTOS_INVALID_STACK_SIZE,  // Fixed from UNVALID
  sRTOS_INVALID_PERIOD,      // Fixed from UNVALID
  sRTOS_ALLOCATION_FAILED,
  sRTOS_TIMER_LIST_IS_FULL
} sRTOS_StatusTypeDef;

/* Task Priority Levels */
enum {
  sPriorityIdle        = -16,
  sPriorityLow         = -2,
  sPriorityBelowNormal = -1,
  sPriorityNormal      = 0,
  sPriorityAboveNormal = 1,
  sPriorityHigh        = 2,
  sPriorityRealtime    = 15
};

#define sPriorityMin sPriorityIdle
#define sPriorityMax sPriorityRealtime

typedef signed char sPriority_t;

/* Task States */
typedef enum {
  sBlocked,
  sRunning,
  sReady,
  sDeleted,
  sWaiting
} sTaskStatus_t;

/* Task Control Block (TCB) */
__attribute__((packed, aligned(4))) struct tcb {
  sUBaseType_t  *stackPt;
  struct tcb    *nextTask;
  sbool_t       fps;                 // Indicates if the task is using the FPU
  sTaskStatus_t status;
  sPriority_t   priority;
  sbool_t       registersSaved;      // Fixed typo: Tells the scheduler to save registers
  sUBaseType_t  *stackBase;          // Used for our Stack Overflow detection!
  struct tcb    *prevTask;
  sUBaseType_t  notificationMessage;
  sbool_t       hasNotification;
  sPriority_t   originalPriority;    // Saves priority before priority inheritance (mutex)
  char          name[SR_MAX_TASK_NAME_LEN];
};

typedef struct tcb sTaskHandle_t;

/* Software Timer Block */
typedef struct __attribute__((packed, aligned(4))) {
  sUBaseType_t  *stackPt;   // Pointer to the stack
  sUBaseType_t  *stackBase; // Pointer to the Base of the stack
  sUBaseType_t  id;         // Timer id
  sBaseType_t   Period;     // Timer period in ticks (relative to SR_TICK_RATE_HZ)
  sbool_t       autoReload; // Timer autoReload flag
  sTaskStatus_t status;
} sTimerHandle_t;

/* Function Pointers & IPC Types */
typedef void (*sTaskFunc_t)(void *arg);
typedef void (*sTimerFunc_t)(sTimerHandle_t *timerHandle);
typedef sBaseType_t sSemaphore_t;

/* Mutex Block */
typedef struct {
  sSemaphore_t   sem;
  sTaskHandle_t  *holderHandle;
  sTaskHandle_t  *requesterHandle;
} sMutex_t;

/* Queue Block */
typedef struct {
  sUBaseType_t  maxLength;  // Fixed typo: maxLenght
  sUBaseType_t  length;     // Fixed typo: lenght
  sUBaseType_t  itemSize;
  sUBaseType_t  index;
  void          **items;
} sQueueHandle_t;

#endif /* SR_TYPES_H_ */