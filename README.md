# freertos-stm32g474-sensor-gateway
This project demonstrates the practical application of a Real-Time Operating System (RTOS)  on an ARM Cortex-M4 microcontroller (STM32G474RE).

Projektbeschreibung

Ziel des Projekts

Dieses Projekt demonstriert die praktische Anwendung eines Echtzeit-Betriebssystems (RTOS) 
auf einem ARM Cortex-M4 Mikrocontroller (STM32G474RE). 

Ziel war es, ein realistisches eingebettetes System zu entwerfen, 
das mehrere Sensoren gleichzeitig überwacht, Messdaten verarbeitet, 
Grenzwertüberschreitungen erkennt und alle Ereignisse über eine serielle Schnittstelle (UART) ausgibt – 
alles unter der Kontrolle von FreeRTOS.

Das System simuliert ein industrielles Multisensor-Gateway, wie es in der Automatisierungs- 
und Fertigungstechnik eingesetzt wird: Sensordaten werden kontinuierlich erfasst, 
priorisiert verarbeitet und bei Anomalien werden Alarme ausgelöst. Ein integrierter Software-Watchdog überwacht 
dabei permanent die Aktivität aller Sensor-Tasks und meldet Ausfälle automatisch.

Das Projekt wurde als Lernprojekt zur Vertiefung der FreeRTOS-Kenntnisse realisiert und deckt 
die wichtigsten RTOS-Konzepte in einem einzigen, zusammenhängenden System ab: Tasks, 
Prioritäten, Queues, Mutex, Binary Semaphore, Counting Semaphore und Software Timer.

Was wurde gelernt und umgesetzt?

Strukturierung eines Embedded-Systems in unabhängige, nebenläufige Tasks
Sicherer Datenaustausch zwischen Tasks über eine Queue
Schutz gemeinsam genutzter Ressourcen (UART-Ausgabepuffer) durch einen Mutex
Task-Synchronisation mittels Binary Semaphore (Alarm-Auslösung)
Begrenzung gleichzeitiger Ereignisse durch Counting Semaphore (max. 3 aktive Alarme)
Periodische Systemüberwachung durch einen FreeRTOS Software Timer (Watchdog)
Serielle Ausgabe über LPUART1 mit direktem Registerzugriff


Project Description

Project Goal

This project demonstrates the practical application of a Real-Time Operating System (RTOS) 
on an ARM Cortex-M4 microcontroller (STM32G474RE). The goal was to design a realistic embedded 
system that simultaneously monitors multiple sensors, processes measurement data, detects threshold 
violations, and outputs all events via a serial interface (UART) — all under the control of FreeRTOS.

The system simulates an industrial multisensor gateway, as used in automation and manufacturing: 
sensor data is continuously acquired, processed with priority scheduling, and alarms are triggered 
upon anomalies. An integrated software watchdog permanently monitors the activity of all sensor tasks 
and automatically reports failures.

This project was developed as a hands-on learning project to deepen FreeRTOS knowledge, 
covering the most important RTOS concepts within a single, cohesive system: Tasks, Priorities, 
Queues, Mutex, Binary Semaphore, Counting Semaphore, and Software Timer.

What was learned and implemented?

Structuring an embedded system into independent, concurrent tasks
Safe data exchange between tasks using a Queue
Protection of shared resources (UART output buffer) via a Mutex
Task synchronization using a Binary Semaphore (alarm triggering)
Limiting simultaneous events via Counting Semaphore (max. 3 active alarms)
Periodic system monitoring through a FreeRTOS Software Timer (watchdog)
Serial output via LPUART1 with direct register Access


Hardware & Software

Component		              Details

Microcontroller		        STM32G474RE (ARM Cortex-M4, 170 MHz)

Board			                NUCLEO-G474RE

RTOS			                FreeRTOS (via CMSIS-RTOS v2)

IDE			                  STM32CubeIDE

HAL 			                LibrarySTM32 HAL (STM32CubeG4)

Serial 			              MonitorRealTerm

Language		              C/C++

UART Interface		        LPUART1 (via ST-Link COM)

System Architecture

┌─────────────────────────────────────────────────────────────┐
│                    FreeRTOS Scheduler                       │
│                                                             │
│  ┌─────────────────┐    ┌─────────────────┐                 │
│  │
     Humidity Sensor │    │ Pressure Sensor │                 │
│  │ 
     Task (Prio: 2)  │    │ Task (Prio: 2)  │                 │
│  └────────┬────────┘    └────────┬────────┘                 │
│           │
│             Watchdog Flags      │
│     	  └──────────┬──────────┘ (volatile uint8_t)        │
│                      │ 
                                     				       │
│                   [QUEUE]                                   │
│                (max. 10 items)                              │
│                      │                                      │
│           ┌──────────▼──────────┐                           │
│           │    Data Processor   │                           │
│           │    Task (Prio: 3)   │ ◄── Highest Priority      │
│           └──────────┬──────────┘                           │
│                      │                                      │
│           ┌───────────┴────────────┐                        │
│           │                        │                        │
│   [Mutex: outputBuffer]      [Binary Semaphore]             │
│           │                        │                        │
│   ┌───────▼────────┐   ┌──────────▼──────────┐              │
│   │  UART TX Task  │   │  Alarm Monitor Task │              │
│   │  (Prio: 1)     │   │  (Prio: 2)          │              │
│   └───────┬────────┘   └──────────┬──────────┘              │
│           │                       │  [Counting Semaphore]   │
│          └───────────┬────────────┘     (max. 3 alarms)     │
│                          │                                  │
│                     [LPUART1 Output]                        │
│                          │                                  │
│    ┌───────────────────▼─────────────────────┐              │
│    │         Software Timer (5s)             │              │
│    │         Watchdog Callback               │              │
│    └─────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────┘


Task Description

1. Humidity Sensor Task & Pressure Sensor Task (Priority 2)

Both tasks share the same function vSensorTask and are differentiated via the pvParameters pointer, 
which passes a pre-initialized Data_t structure identifying the sensor type.

Responsibilities:

Simulate reading sensor data using readSensorData() (random values within realistic ranges)
Pack the measured value into a Data_t structure containing sensor value and source identifier
Send the structure to the shared Queue using xQueueSend()
Set the corresponding Watchdog Flag (watchdogFlag_Humidity or watchdogFlag_Pressure) on successful send
Sleep for 500 ms (vTaskDelay) before next measurement

Sensor ranges (simulated):
Humidity: 20–29 %
Pressure: 30–39 hPa

2. Data Processor Task (Priority 3 – Highest)
The highest priority task in the system. It waits permanently for new data in the Queue 
and processes it immediately upon arrival.

Responsibilities:

Receive Data_t structures from the Queue using xQueueReceive(portMAX_DELAY)
Determine the correct threshold based on sensor type (HUMIDITY_THRESHOLD or PRESSURE_THRESHOLD)
Acquire the Mutex to safely write a formatted status message into outputBuffer
Release the Mutex after writing
If threshold is exceeded: copy data to alarmData and release the Binary Semaphore 
   to wake up the Alarm Monitor Task

3. UART TX Task (Priority 1 – Lowest)

Responsible exclusively for serial output. Low priority ensures it never interferes 
with data acquisition or processing.

Responsibilities:

Acquire the Mutex to safely read from outputBuffer
Output the buffer content via printf() → routed through __io_putchar() → LPUART1
Release the Mutex
Sleep 100 ms before next output cycle

4. Alarm Monitor Task (Priority 2)

Waits indefinitely for the Binary Semaphore to be released by the Data Processor Task.

Responsibilities:

Block on xSemaphoreTake(binarySemaphrHandle, portMAX_DELAY)
Upon alarm: increment the Counting Semaphore (tracks active alarms, max. 3)
Acquire the Mutex to write a formatted alarm message into outputBuffer
Display current active alarm count using uxSemaphoreGetCount()
Release Mutex and Counting Semaphore

5. Watchdog Timer Callback (Software Timer, 5s interval)

A FreeRTOS Software Timer running in auto-reload mode. Its callback executes in 
the context of the Timer Daemon Task — not an ISR — so standard semaphore functions 
are used (not FromISR variants).

Responsibilities:

Every 5 seconds: check watchdogFlag_Humidity and watchdogFlag_Pressure
If both flags are set → write [WATCHDOG] OK to output buffer
If any flag is missing → write [WATCHDOG] WARNING! <sensor> not active
Reset both flags to 0 for the next monitoring cycle

Synchronization Concepts

Mechanism		               Used for

Queue			                 Data transfer from Sensor Tasks → Data Processor

Mutex			                 Exclusive access to shared outputBuffer

Binary Semaphore	         Signal from Data Processor → Alarm Monitor (alarm event)

Counting Semaphore	       Track number of active alarms (max. 3)

Software Timer		         Periodic watchdog check every 5 seconds

Watchdog Flags		         volatile uint8_t flags set by sensor Tasks


Task priorities:

TaskPriority		        Stack Size		  Data 

Processor Task		      3			          512 words

Humidity Sensor Task	  2			          256 words

Pressure Sensor Task	  2			          256 words

Alarm Monitor Task	    2			          256 words

UART TX Task		        1			          256 words


Author:
Dylann Kinfack
M.sc Eletrical Engineering
