# srOS - Lightweight Embedded RTOS

**srOS** is a lightweight, preemptive Real-Time Operating System (RTOS) designed for **ARM Cortex-M4** microcontrollers. It provides deterministic task scheduling, efficient inter-task communication, synchronization primitives, and software timers while maintaining a minimal memory footprint.

---

## 🚀 Features

- **Preemptive & Cooperative Scheduling**
  - Configurable preemptive or cooperative scheduling.
  - Round-robin execution for tasks with equal priority.

- **Proactive Stack Safety**
  - Runtime stack overflow detection using **0xDEADBEEF** boundary verification.
  - Helps prevent memory corruption caused by stack overflows.

- **Inter-Process Communication (IPC)**
  - Thread-safe circular queues.
  - Lightweight direct-to-task notifications.

- **Synchronization Primitives**
  - Counting Semaphores
  - Mutexes with **Priority Inheritance** to prevent priority inversion.

- **Software Timers**
  - One-shot timers
  - Auto-reload timers

- **Zero-Latency Interrupt Support**
  - ISR-safe APIs such as:
    - `sRTOSQueueSendFromISR()`
    - `sRTOSMutexGiveFromISR()`

---

## 📁 Repository Structure

```text
├── inc/
│   ├── sr_api.h           # Public RTOS API
│   ├── sr_config.h        # Kernel configuration
│   └── sr_types.h         # Core data types
│
├── src/
│   ├── sr_port_cm4.s      # Cortex-M4 context switching
│   ├── sr_scheduler.c     # Scheduler
│   ├── sr_task.c          # Task management
│   ├── sr_queue.c         # Queue implementation
│   ├── sr_semaphore.c     # Mutex & semaphore
│   ├── sr_delay.c         # Delay management
│   ├── sr_timer.c         # Software timers
│   ├── sr_notify.c        # Task notifications
│   └── sr_interrupts.c    # SysTick & SVC handlers
```

---

## ⚙️ Configuration

Configure the kernel in **`sr_config.h`**.

| Macro | Description |
|-------|-------------|
| `SR_TICK_RATE_HZ` | System tick frequency (e.g. `1000` = 1 ms tick) |
| `SR_USE_PREEMPTION` | `1` = Preemptive, `0` = Cooperative |
| `SR_TIME_SLICE_TICKS` | Time slice for tasks with equal priority |

---

## 💻 Example Application

The following example demonstrates a **Producer-Consumer** architecture using RTOS queues.

```c
#include "sr_api.h"

sQueueHandle_t sensorQueue;
sTaskHandle_t producerHandle, consumerHandle;

// Producer Task
void Task_SensorProducer(void *arg)
{
    uint32_t sensorValue = 0;

    while (1)
    {
        sensorValue = ReadHardwareADC();

        sRTOSQueueSend(&sensorQueue, &sensorValue, 10);

        sRTOSTaskDelay(50);
    }
}

// Consumer Task
void Task_DataConsumer(void *arg)
{
    uint32_t receivedData = 0;

    while (1)
    {
        if (sRTOSQueueReceive(&sensorQueue, &receivedData, 100))
        {
            ProcessData(receivedData);
        }

        if (sRTOSCheckStackOverflow(&producerHandle) != sRTOS_OK)
        {
            SystemErrorHandler();
        }
    }
}

int main(void)
{
    sRTOSInit(16000000);

    sRTOSQueueCreate(&sensorQueue, 10, sizeof(uint32_t));

    sRTOSTaskCreate(Task_SensorProducer,
                    "Producer",
                    NULL,
                    128,
                    1,
                    &producerHandle,
                    SR_FALSE);

    sRTOSTaskCreate(Task_DataConsumer,
                    "Consumer",
                    NULL,
                    256,
                    2,
                    &consumerHandle,
                    SR_FALSE);

    sRTOSStartScheduler();

    while (1);

    return 0;
}
```

---

## 🏗️ ARM Cortex-M4 Architecture

The kernel leverages standard ARM Cortex-M4 hardware features.

### SysTick

- Generates the RTOS system tick.
- Drives delays, scheduling, and time slicing.

### PendSV

- Performs context switching.
- Runs at the lowest interrupt priority to avoid delaying critical ISRs.

### SVC (Supervisor Call)

Used for privileged kernel operations such as:

- Starting the first task
- Cooperative task yielding

### FPU Support

Tasks can optionally enable hardware floating-point support (`fpsMode`).

Only tasks that use floating-point operations save and restore **s0–s31** registers, reducing context-switch overhead.

---

## 📌 Highlights

- Lightweight RTOS kernel
- Deterministic scheduling
- Stack overflow protection
- Queue-based IPC
- Task notifications
- Mutexes with Priority Inheritance
- Counting semaphores
- Software timers
- ARM Cortex-M4 optimized
- ISR-safe APIs

---

## 📄 License

This project is intended for educational and embedded systems development purposes.
