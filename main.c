/*
 * main.c
 *
 * srOS Kernel Validation & Stress Test
 */

#include <stdint.h>
#include "stm32f4xx.h" // CMSIS Device Header for Cortex-M4
#include "sr_kernel.h"  // Refactored srOS API

extern uint32_t SystemCoreClock;

/* Global execution counters for debug verification */
uint32_t count0 = 0, count1 = 0, count2 = 0, count3 = 0, count4 = 0;
uint32_t timercount0 = 0, timercount1 = 0, timercount2 = 0;

/* RTOS Object Handles */
sTaskHandle_t Task0H;
sTaskHandle_t Task1H;
sTaskHandle_t Task2H;
sTaskHandle_t Task3H;
sTaskHandle_t Task4H;
sTimerHandle_t Timer0H;
sTimerHandle_t Timer1H;
sTimerHandle_t Timer2H;

/* * Timer Callbacks 
 */
void Timer0(sTimerHandle_t *h)
{
  timercount0++;
}

void Timer1(sTimerHandle_t *h)
{
  timercount1++;
}

void Timer2(sTimerHandle_t *h)
{
  timercount2++;
}

/* * Task Definitions 
 */
void Task0(void *arg)
{
  (void)arg;
  while (1)
  {
    count0++;
    // Demonstrate dynamic task control: Stop Task4 after counting to 100,000
    if (count0 == 100000)
    {
      sRTOSTaskStop(&Task4H);
    }
  }
}

void Task1(void *arg)
{
  (void)arg;
  while (1)
  {
    count1++;
  }
}

void Task2(void *arg)
{
  (void)arg;
  while (1)
  {
    count2++;
  }
}

void Task3(void *arg)
{
  (void)arg;
  sRTOSTaskDelay(5);
  while (1)
  {
    count3++;
    if ((count3 % 1000) == 0)
    {
      sRTOSTaskDelay(5); // Yield execution periodically
    }
  }
}

void Task4(void *arg)
{
  (void)arg;
  sRTOSTaskDelay(10);
  while (1)
  {
    count4++;
    if ((count4 % 1000) == 0)
    {
      sRTOSTaskDelay(5); // Yield execution periodically
    }
  }
}

/* * Application Entry Point 
 */
int main(void)
{
  // 1. Initialize core system clock (CMSIS standard)
  SystemCoreClockUpdate();
  
  // 2. Initialize the srOS Kernel with the current bus frequency
  sRTOSInit(SystemCoreClock);

  // 3. Create Application Tasks (Function, Name, Args, Stack, Priority, Handle, FPU_Mode)
  sRTOSTaskCreate(Task4, "Task4", NULL, 128, sPriorityHigh,   &Task4H, SR_FALSE);
  sRTOSTaskCreate(Task3, "Task3", NULL, 128, sPriorityHigh,   &Task3H, SR_FALSE);
  sRTOSTaskCreate(Task2, "Task2", NULL, 128, sPriorityNormal, &Task2H, SR_TRUE);
  sRTOSTaskCreate(Task1, "Task1", NULL, 128, sPriorityNormal, &Task1H, SR_TRUE);
  sRTOSTaskCreate(Task0, "Task0", NULL, 128, sPriorityNormal, &Task0H, SR_FALSE);

  // 4. Create Software Timers (Callback, ID, Period, AutoReload, Handle)
  sRTOSTimerCreate(Timer0, 1, 80,  SR_TRUE,  &Timer0H);  // Auto-reloading every 80 ticks
  sRTOSTimerCreate(Timer1, 1, 160, SR_TRUE,  &Timer1H);  // Auto-reloading every 160 ticks
  sRTOSTimerCreate(Timer2, 1, 5,   SR_FALSE, &Timer2H);  // One-shot timer

  // 5. Transfer CPU control to the scheduler
  sRTOSStartScheduler();
  
  // Execution should never reach here
  while(1);
  return 0;
}
