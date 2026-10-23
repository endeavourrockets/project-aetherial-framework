/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdbool.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
    uint32_t timestamp_ms;
    int32_t pressure_raw;
    int32_t accel_raw[3];
} raw_reading_t;

typedef struct
{
    uint32_t timestamp_ms;
    bool valid;
    int32_t altitude_cm;
    int32_t accel_mg[3];
} processed_reading_t;

typedef struct 
{
  uint8_t id;
  uint8_t argument;
} command_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
QueueHandle_t log_queue;
volatile uint32_t readings_sent = 0;
volatile uint32_t readings_logged = 0;
QueueHandle_t telemetry_queue;
volatile uint32_t readings_sent_to_ground = 0;
volatile bool system_healthy = true;
QueueHandle_t command_queue;
volatile uint32_t commands_handled = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void start_readings(void)
{

}
static bool get_raw_readings(raw_reading_t *raw)
{
  raw->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
  raw->pressure_raw = 101325;
  raw->accel_raw[0] = 0;
  raw->accel_raw[1] = 0;
  raw->accel_raw[2] = 1000;
  return true;
}
static bool validate(const raw_reading_t *raw)
{
  return raw->pressure_raw > 1000 && raw->pressure_raw < 120000;
}
static void process(const raw_reading_t *raw, processed_reading_t *out)
{
  out->timestamp_ms = raw->timestamp_ms;
  out->valid = true;
  out->altitude_cm = 0;
  out->accel_mg[0] = raw->accel_raw[0];
  out->accel_mg[1] = raw->accel_raw[1];
  out->accel_mg[2] = raw->accel_raw[2];
}
static void sd_log(const processed_reading_t *reading)
{

}
static void radio_send(const processed_reading_t *reading)
{

}
static void sensor_task(void *argument)
{
  start_readings();

  for (;;)
  {
    raw_reading_t raw;
    processed_reading_t processed;

    if (system_healthy)
    {
      if (get_raw_readings(&raw))
      {
        if (validate(&raw))
        {
          process(&raw, &processed);
        }
        else
        {
          processed.timestamp_ms = raw.timestamp_ms;
          processed.valid = false;
          processed.altitude_cm = 0;
          processed.accel_mg[0] = raw.accel_raw[0];
          processed.accel_mg[1] = raw.accel_raw[1];
          processed.accel_mg[2] = raw.accel_raw[2];
        }

        xQueueSend(log_queue, &processed, portMAX_DELAY);
        readings_sent++;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
static void logging_task(void *argument) 
{
  for(;;) 
  {
    processed_reading_t reading;
    if (xQueueReceive(log_queue, &reading, portMAX_DELAY) == pdPASS) 
    {
      sd_log(&reading);
      xQueueSend(telemetry_queue, &reading, 0); 
      readings_logged++;
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);
    }
  }
}
static void telemetry_task(void *argument) 
{
  for(;;) 
  {
    processed_reading_t reading;
    if (xQueueReceive(telemetry_queue, &reading, portMAX_DELAY) == pdPASS) 
    {
      radio_send(&reading);
      readings_sent_to_ground++;
    }
  }
}
static bool core_health_ok(void) 
{
  return true;
}
static void health_task(void *argument) 
{
  for(;;) 
  {
    system_healthy = core_health_ok();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
static void execute_command(const command_t *command)
{

}
static void command_task(void *argument)
{
  for (;;)
  {
    command_t command;

    if (xQueueReceive(command_queue, &command, portMAX_DELAY) == pdPASS)
    {
      execute_command(&command);
      commands_handled++;
    }
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* USER CODE BEGIN 2 */

  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef led = {0};
  led.Pin = GPIO_PIN_8;
  led.Mode = GPIO_MODE_OUTPUT_PP;
  led.Pull = GPIO_NOPULL;
  led.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &led);
  log_queue = xQueueCreate(8, sizeof(processed_reading_t));
  telemetry_queue = xQueueCreate(8, sizeof(processed_reading_t));
  command_queue = xQueueCreate(4, sizeof(command_t));
  xTaskCreate(health_task, "Health Task", 256, NULL, 1, NULL);
  xTaskCreate(sensor_task, "Sensor Task", 256, NULL, 1, NULL);
  xTaskCreate(logging_task, "Logging Task", 256, NULL, 1, NULL);
  xTaskCreate(telemetry_task, "Telemetry Task", 256, NULL, 1, NULL);
  xTaskCreate(command_task, "Command Task", 256, NULL, 1, NULL);
  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
#ifdef USE_FULL_ASSERT
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
