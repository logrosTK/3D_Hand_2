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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HAND_SERVO_COUNT          5U
#define HAND_PWM_PERIOD_US        20000U
#define HAND_CONTROL_PERIOD_MS    20U
#define HAND_COMMAND_TIMEOUT_MS   500U
#define HAND_UART_BAUDRATE        115200U
#define HAND_RX_RING_SIZE         128U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

static volatile uint8_t rx_byte;
static volatile uint8_t rx_ring[HAND_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

static const uint16_t servo_open_us[HAND_SERVO_COUNT] = {
  1000U, 1000U, 1000U, 1000U, 1000U
};

static const uint16_t servo_close_us[HAND_SERVO_COUNT] = {
  1900U, 2000U, 2000U, 1950U, 1900U
};

static uint16_t servo_current_us[HAND_SERVO_COUNT];
static uint16_t servo_target_us[HAND_SERVO_COUNT];
static uint32_t last_command_ms;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void Hand_Init(void);
static void Hand_Tick(uint32_t now_ms);
static void Hand_SetGrip(float grip, uint32_t now_ms);
static void Hand_SetFingers(const float fingers[HAND_SERVO_COUNT], uint32_t now_ms);
static void Hand_WriteServoUs(uint8_t servo_id, uint16_t pulse_us);
static void Hand_ProcessLine(const char *line);
static void Hand_UartWrite(const char *text);
static void Hand_UartWriteStatus(void);
static bool Hand_RxPop(uint8_t *out);
static bool Hand_ParseUnitValue(const char **cursor, float *out);
static float Hand_Clamp01(float value);
static void Hand_StartUartRx(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  Hand_Init();
  Hand_StartUartRx();
  Hand_UartWrite("3D_HAND_READY\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static char line[96];
    static uint8_t line_len = 0U;
    static uint32_t next_control_ms = 0U;
    uint8_t byte;

    while (Hand_RxPop(&byte))
    {
      if (byte == '\r')
      {
        continue;
      }
      if (byte == '\n')
      {
        line[line_len] = '\0';
        Hand_ProcessLine(line);
        line_len = 0U;
      }
      else if (line_len < (sizeof(line) - 1U))
      {
        line[line_len++] = (char)byte;
      }
      else
      {
        line_len = 0U;
      }
    }

    uint32_t now_ms = HAL_GetTick();
    if ((int32_t)(now_ms - next_control_ms) >= 0)
    {
      Hand_Tick(now_ms);
      next_control_ms = now_ms + HAND_CONTROL_PERIOD_MS;
    }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void MX_USART1_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart1.Instance = USART1;
  huart1.Init.BaudRate = HAND_UART_BAUDRATE;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void Hand_PWM_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_TIM2_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();
  Hand_PWM_GPIO_Init();

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 72U - 1U;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = HAND_PWM_PERIOD_US - 1U;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000U;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
}

static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  Hand_PWM_GPIO_Init();

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 72U - 1U;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = HAND_PWM_PERIOD_US - 1U;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000U;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

static void Hand_Init(void)
{
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    servo_current_us[i] = servo_open_us[i];
    servo_target_us[i] = servo_open_us[i];
    Hand_WriteServoUs(i, servo_current_us[i]);
  }
  last_command_ms = HAL_GetTick();
}

static void Hand_Tick(uint32_t now_ms)
{
  if ((now_ms - last_command_ms) > HAND_COMMAND_TIMEOUT_MS)
  {
    for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
    {
      servo_target_us[i] = servo_open_us[i];
    }
  }

  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    if (servo_current_us[i] < servo_target_us[i])
    {
      uint16_t delta = servo_target_us[i] - servo_current_us[i];
      servo_current_us[i] += (delta > 10U) ? 10U : delta;
    }
    else if (servo_current_us[i] > servo_target_us[i])
    {
      uint16_t delta = servo_current_us[i] - servo_target_us[i];
      servo_current_us[i] -= (delta > 10U) ? 10U : delta;
    }

    Hand_WriteServoUs(i, servo_current_us[i]);
  }
}

static void Hand_SetGrip(float grip, uint32_t now_ms)
{
  grip = Hand_Clamp01(grip);
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    servo_target_us[i] = (uint16_t)((float)servo_open_us[i] +
                         ((float)servo_close_us[i] - (float)servo_open_us[i]) * grip);
  }
  last_command_ms = now_ms;
}

static void Hand_SetFingers(const float fingers[HAND_SERVO_COUNT], uint32_t now_ms)
{
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    float value = Hand_Clamp01(fingers[i]);
    servo_target_us[i] = (uint16_t)((float)servo_open_us[i] +
                         ((float)servo_close_us[i] - (float)servo_open_us[i]) * value);
  }
  last_command_ms = now_ms;
}

static void Hand_WriteServoUs(uint8_t servo_id, uint16_t pulse_us)
{
  if (pulse_us < 500U)
  {
    pulse_us = 500U;
  }
  else if (pulse_us > 2500U)
  {
    pulse_us = 2500U;
  }

  switch (servo_id)
  {
    case 0: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us); break;
    case 1: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse_us); break;
    case 2: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse_us); break;
    case 3: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pulse_us); break;
    case 4: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse_us); break;
    default: break;
  }
}

static void Hand_ProcessLine(const char *line)
{
  if (line == NULL || line[0] == '\0')
  {
    return;
  }

  if (strcmp(line, "OPEN") == 0 || strcmp(line, "O") == 0)
  {
    Hand_SetGrip(0.0f, HAL_GetTick());
    Hand_UartWrite("OK,OPEN\r\n");
    return;
  }

  if (strcmp(line, "CLOSE") == 0 || strcmp(line, "C") == 0)
  {
    Hand_SetGrip(1.0f, HAL_GetTick());
    Hand_UartWrite("OK,CLOSE\r\n");
    return;
  }

  if (strcmp(line, "PING") == 0)
  {
    Hand_UartWrite("PONG\r\n");
    return;
  }

  if (strcmp(line, "STATUS") == 0 || strcmp(line, "S") == 0)
  {
    Hand_UartWriteStatus();
    return;
  }

  if (line[0] == 'G' && line[1] == ',')
  {
    const char *cursor = line + 2;
    float grip = 0.0f;
    if (Hand_ParseUnitValue(&cursor, &grip) && *cursor == '\0')
    {
      Hand_SetGrip(grip, HAL_GetTick());
      Hand_UartWrite("OK,G\r\n");
      return;
    }
    Hand_UartWrite("ERR,G\r\n");
    return;
  }

  if (line[0] == 'F' && line[1] == ',')
  {
    const char *cursor = line + 2;
    float fingers[HAND_SERVO_COUNT];
    for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
    {
      if (!Hand_ParseUnitValue(&cursor, &fingers[i]))
      {
        return;
      }
      if (i + 1U < HAND_SERVO_COUNT)
      {
        if (*cursor != ',')
        {
          return;
        }
        cursor++;
      }
    }
    if (*cursor != '\0')
    {
      Hand_UartWrite("ERR,F\r\n");
      return;
    }
    Hand_SetFingers(fingers, HAL_GetTick());
    Hand_UartWrite("OK,F\r\n");
    return;
  }

  Hand_UartWrite("ERR,UNKNOWN\r\n");
}

static void Hand_UartWrite(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 50U);
}

static void Hand_UartWriteStatus(void)
{
  char text[96];
  int len = snprintf(text, sizeof(text),
                     "STATUS,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
                     (unsigned int)servo_current_us[0],
                     (unsigned int)servo_current_us[1],
                     (unsigned int)servo_current_us[2],
                     (unsigned int)servo_current_us[3],
                     (unsigned int)servo_current_us[4],
                     (unsigned int)servo_target_us[0],
                     (unsigned int)servo_target_us[1],
                     (unsigned int)servo_target_us[2],
                     (unsigned int)servo_target_us[3],
                     (unsigned int)servo_target_us[4]);
  if (len > 0 && len < (int)sizeof(text))
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)len, 50U);
  }
}

static bool Hand_RxPop(uint8_t *out)
{
  if (rx_tail == rx_head)
  {
    return false;
  }
  *out = rx_ring[rx_tail];
  rx_tail = (uint16_t)((rx_tail + 1U) % HAND_RX_RING_SIZE);
  return true;
}

static bool Hand_ParseUnitValue(const char **cursor, float *out)
{
  uint32_t whole = 0U;
  uint32_t frac = 0U;
  uint32_t frac_scale = 1U;
  bool has_digit = false;
  const char *p = *cursor;

  while (*p >= '0' && *p <= '9')
  {
    has_digit = true;
    whole = whole * 10U + (uint32_t)(*p - '0');
    p++;
  }

  if (*p == '.')
  {
    p++;
    while (*p >= '0' && *p <= '9')
    {
      has_digit = true;
      if (frac_scale < 1000000U)
      {
        frac = frac * 10U + (uint32_t)(*p - '0');
        frac_scale *= 10U;
      }
      p++;
    }
  }

  if (!has_digit)
  {
    return false;
  }

  *out = Hand_Clamp01((float)whole + ((float)frac / (float)frac_scale));
  *cursor = p;
  return true;
}

static float Hand_Clamp01(float value)
{
  if (value < 0.0f)
  {
    return 0.0f;
  }
  if (value > 1.0f)
  {
    return 1.0f;
  }
  return value;
}

static void Hand_StartUartRx(void)
{
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    uint16_t next_head = (uint16_t)((rx_head + 1U) % HAND_RX_RING_SIZE);
    if (next_head != rx_tail)
    {
      rx_ring[rx_head] = rx_byte;
      rx_head = next_head;
    }
    Hand_StartUartRx();
  }
}

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
