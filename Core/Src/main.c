
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "gpio.h"

#define BUFFERSIZE 150
#define HUMIDITY_THRESHOLD 80 //%
#define PRESSURE_THRESHOLD 50 // hPa



void SystemClock_Config(void);
void MX_FREERTOS_Init(void);

int __io_putchar(int ch);

/*Define the enumerate type for two different sensor */
typedef enum
{
	humidity_sensor,
	pressure_sensor

}sensor_t;

/*Define the sensor Data structure type to be passed to the queue*/
typedef struct
{
	int sensor_value;
	sensor_t sDatasource;
}Data_t;

typedef uint32_t Taskprofiler;

Taskprofiler ReceiveProfiler, SendProfiler, Full;

//Declare two "Data_t" variables that will be passed to the queue
static  Data_t xStructsToSend[2] =
{
		{ 37, humidity_sensor}, // Used by humidity sensor
		{ 45, pressure_sensor} //  used by pressure sensor
};


//declare the task handler for humidity, pressure and data processor task
TaskHandle_t humidityTaskHandle, pressureTaskHandle, dataProcessorTaskHandle;

void readSensorData(Data_t* pData, const sensor_t* sensorName);

QueueHandle_t queue_handle;
TimerHandle_t timerHandle;
SemaphoreHandle_t mutexHandle;
SemaphoreHandle_t countingSemaphrHandle;
SemaphoreHandle_t binarySemaphrHandle;


char outputBuffer[BUFFERSIZE]; // Output Buffer to send via UART
Data_t alarmData; // Struct Alarm Data

// Watchdog Flags – each Sensor-Task set his Flag after successfull send
volatile uint8_t watchdogFlag_Humidity = 0;
volatile uint8_t watchdogFlag_Pressure = 0;



void vSensorTask(void *pvParameters);
void vDataProcessorTask(void *pvParameters);
void vUartTxTask(void* pvParameters);
void vAlarmMonitorTask(void* pvParameters);
void vWatchdogCallback(TimerHandle_t xTimer);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_LPUART1_UART_Init();

  //Create queue to hold a maximum of 10 structures
  queue_handle = xQueueCreate(10, sizeof(Data_t));

  mutexHandle = xSemaphoreCreateMutex();
  binarySemaphrHandle = xSemaphoreCreateBinary();

  countingSemaphrHandle = xSemaphoreCreateCounting(3,0);

  timerHandle = xTimerCreate("WachtdogTimer", // Name
		  	      pdMS_TO_TICKS(5000), // period 5s
			      pdTRUE, // Auto reload Mode True
			      NULL, // No Timer ID
			      vWatchdogCallback // Callback Funktion
			    );

  //Create the different task with their priorities
  xTaskCreate(vDataProcessorTask,
		  	  "Data Processor Task",
			  512,
			  NULL,
			  3,
			  &dataProcessorTaskHandle);

  xTaskCreate(vSensorTask,
		  	  "Humidity Sensor Task",
			  256,
			  (void *)&(xStructsToSend[0]),
			  2,
			  &humidityTaskHandle);

  xTaskCreate(vSensorTask,
  		  	  "Pressure Sensor Task",
  			  256,
  			  (void *)&(xStructsToSend[1]),
  			  2,
  			  &pressureTaskHandle);

  xTaskCreate(vUartTxTask,
		  	  "UART TX Task",
		  	  256,
			  NULL,
			  1,
			  NULL);

  xTaskCreate(vAlarmMonitorTask,
		  	  "Alarm Monitor Task",
		  	  256,
			  NULL,
			  2,
			  NULL);

  vTaskStartScheduler();

  while (1)
  {


  }

}


void vWatchdogCallback(TimerHandle_t xTimer)
{
	if(xSemaphoreTake(mutexHandle, portMAX_DELAY)== pdPASS)
	{

		if(watchdogFlag_Humidity == 1 && watchdogFlag_Pressure == 1)
		{
			sprintf(outputBuffer, "[WATCHDOG] OK - All sensor active \r\n");
		}
		else
		{
			sprintf(outputBuffer, "[WATCHDOG] WARNING! %s%s sensor not active \r\n",
					watchdogFlag_Humidity == 0 ? "Humidity" : "",
					watchdogFlag_Pressure == 0 ? "Pressure" : "");
		}

		watchdogFlag_Humidity =0;
		watchdogFlag_Pressure =0;

		xSemaphoreGive(mutexHandle);
	}
}

void readSensorData(Data_t* pData, const sensor_t* sensorName)
{
	if(*sensorName == humidity_sensor)
	{
		pData->sensor_value = 20 + (rand()%10);
		pData->sDatasource = humidity_sensor;
	}
	else
	{
		pData->sensor_value = 30 + (rand() % 10);
		pData->sDatasource = pressure_sensor;
	}
}


void vSensorTask(void *pvParameters)
{
	BaseType_t qStatus; // xQueueSend status
	Data_t data; // Struct sensor data to send in the queue

	while(1)
	{

		Data_t *pInputData = (Data_t *)pvParameters;

		/* Read the Sensor values and store them in the Queue*/
		data = *pInputData;
		readSensorData(&data, &data.sDatasource);

		qStatus = xQueueSend(queue_handle, &data, portMAX_DELAY);

		if(qStatus != pdPASS)
		{
			xSemaphoreTake(mutexHandle, portMAX_DELAY);
			printf("Queue Full");
			xSemaphoreGive(mutexHandle);
		}
		else
		{
			if(data.sDatasource == humidity_sensor)
			{
				watchdogFlag_Humidity = 1;
			}
			else
			{
				watchdogFlag_Pressure = 1;
			}
		}

		vTaskDelay(500);
	}
}

void vDataProcessorTask(void *pvParameters)
{
	Data_t receivedQueueData;
	BaseType_t queueStatus, semaphStatus;

	int threshold;

	while(1)
	{

		queueStatus = xQueueReceive(queue_handle, &receivedQueueData, portMAX_DELAY);

		if(queueStatus == pdPASS)
		{
			threshold = (receivedQueueData.sDatasource == humidity_sensor)
					? HUMIDITY_THRESHOLD : PRESSURE_THRESHOLD;

			semaphStatus = xSemaphoreTake(mutexHandle, portMAX_DELAY);

			if(semaphStatus == pdPASS)
			{
				sprintf(outputBuffer, "[SENSOR] %s : %d %s STATUS: %s \r\n",
					receivedQueueData.sDatasource == humidity_sensor ? "Humidity " : "Pressure ",
					receivedQueueData.sensor_value,
					receivedQueueData.sDatasource == humidity_sensor ? "percent" : "hPa",
					receivedQueueData.sensor_value > threshold ? "ALARM" : "OK");

				xSemaphoreGive(mutexHandle);

				if(receivedQueueData.sensor_value > threshold)
				{
					alarmData = receivedQueueData;
					xSemaphoreGive(binarySemaphrHandle);
				}
			}
		}
		vTaskDelay(200);
	}
}

void vUartTxTask(void* pvParameters)
{
	BaseType_t status;

	while(1)
	{
		status = xSemaphoreTake(mutexHandle, portMAX_DELAY);
		if(status == pdPASS)
		{
			printf(outputBuffer);
			xSemaphoreGive(mutexHandle);
		}
		vTaskDelay(100);
	}
}
void vAlarmMonitorTask(void* pvParameters)
{
	char* unit;
	char* sensorName;
	int threshold;
	BaseType_t qStatus;

	while(1)
	{
		qStatus = xSemaphoreTake(binarySemaphrHandle, portMAX_DELAY);

		if(qStatus == pdPASS)
		{

			if(alarmData.sDatasource == humidity_sensor)
			{
				sensorName = "Humidity";
				unit = "%";
				threshold = HUMIDITY_THRESHOLD;
			}
			else
			{
				sensorName = "Pressure";
				unit="hPa";
				threshold = PRESSURE_THRESHOLD;
			}

			xSemaphoreTake(countingSemaphrHandle, portMAX_DELAY);

			qStatus = xSemaphoreTake(mutexHandle, portMAX_DELAY);
			if(qStatus == pdPASS)
			{
				snprintf(outputBuffer, sizeof(outputBuffer),
						"===========================\r\n"
						"*** ALARM *** %s zu hoch: %d%s (Limit: %d)\r\n"
						"Aktive Alarme: %d/3 \r\n"
						"===========================\r\n",
						sensorName,
						alarmData.sensor_value,
						unit,
						threshold,
						uxSemaphoreGetCount(countingSemaphrHandle));

				xSemaphoreGive(mutexHandle);
				xSemaphoreGive(countingSemaphrHandle);
			}

		}
		vTaskDelay(20);
	}
}

int uart1_write(int ch)
{
	while(!(hlpuart1.Instance->ISR & 0x0080)){}
	hlpuart1.Instance->TDR = (ch & 0xFF);
	return ch;

}

int __io_putchar(int ch)
{
	 uart1_write(ch);
	//HAL_UART_Transmit(&hlpuart1, (uint8_t*)&ch, 1, 0xFFFF);
	return ch;

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
