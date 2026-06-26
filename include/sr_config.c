/*
 * sr_config.h
 *
 * RTOS System Configuration Definitions
 */

#ifndef SR_CONFIG_H_
#define SR_CONFIG_H_

/* * Kernel Tick Rate (System Responsiveness) 
 * Defines the frequency of the RTOS tick interrupt in Hz.
 * Note: Functions using MS as input rely on this baseline frequency.
 */
#define SR_TICK_RATE_100HZ      100     // 10ms tick
#define SR_TICK_RATE_1000HZ     1000    // 1ms tick
#define SR_TICK_RATE_2000HZ     2000    // 500us tick
#define SR_TICK_RATE_4000HZ     4000    // 250us tick
#define SR_TICK_RATE_10000HZ    10000   // 100us tick

// Set the active system tick rate here
#define SR_TICK_RATE_HZ         SR_TICK_RATE_2000HZ

/*
 * Scheduler Configuration
 */
#define SR_USE_PREEMPTION       1       // 1: Preemptive scheduling, 0: Cooperative
#define SR_TIME_SLICE_TICKS     2       // Time quantum for Round-Robin scheduling (in ticks)

/*
 * Task & Memory Configuration
 */
#define SR_TIMER_TASK_STACK_WORDS  256  // Stack depth for the daemon timer task (in 32-bit words)
    
/*
 * System Constants
 */
#define SR_MAX_DELAY            0xFFFFFFFFUL // Maximum block time for yielding tasks

#endif /* SR_CONFIG_H_ */
