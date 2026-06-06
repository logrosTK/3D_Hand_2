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
// 标准库头文件：bool类型、字符串处理、格式化输出
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// ---- 灵巧手控制参数宏定义 ----
#define HAND_SERVO_COUNT          5U      // 舵机数量：5根手指
#define HAND_PWM_PERIOD_US        20000U  // PWM周期20ms（50Hz），SG90舵机标准频率
#define HAND_CONTROL_PERIOD_MS    20U     // 控制循环周期20ms，每周期更新一次舵机位置
#define HAND_COMMAND_TIMEOUT_MS   500U    // 命令超时500ms，超时后自动张开手指（安全保护）
#define HAND_UART_BAUDRATE        115200U // 串口波特率115200
#define HAND_RX_RING_SIZE         128U    // 串口接收环形缓冲区大小

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// ---- HAL 外设句柄 ----
UART_HandleTypeDef huart1;   // USART1 串口句柄（与上位机通信）
TIM_HandleTypeDef htim2;     // TIM2 定时器句柄（输出4路PWM：CH1~CH4）
TIM_HandleTypeDef htim3;     // TIM3 定时器句柄（输出1路PWM：CH1）

// ---- 串口接收环形缓冲区 ----
static volatile uint8_t rx_byte;                           // 单字节中断接收暂存
static volatile uint8_t rx_ring[HAND_RX_RING_SIZE];        // 环形缓冲区存储数组
static volatile uint16_t rx_head;                          // 缓冲区写指针（由中断填充）
static volatile uint16_t rx_tail;                          // 缓冲区读指针（由主循环消费）

// ---- 舵机位置表（单位：微秒 us） ----
static const uint16_t servo_open_us[HAND_SERVO_COUNT] = {
  1000U, 1000U, 1000U, 1000U, 1000U   // 张开位置：5个舵机分别对应的PWM脉宽
};

static const uint16_t servo_close_us[HAND_SERVO_COUNT] = {
  1900U, 2000U, 2000U, 1950U, 1900U  // 闭合位置：每根手指机械行程不同，闭合脉宽略有差异
};

static uint16_t servo_current_us[HAND_SERVO_COUNT];   // 当前实际PWM脉宽（平滑运动用）
static uint16_t servo_target_us[HAND_SERVO_COUNT];    // 目标PWM脉宽（命令设定的最终位置）
static uint32_t last_command_ms;                      // 最后一次收到有效命令的时间戳（ms）

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

// ---- 外设初始化函数 ----
static void MX_USART1_UART_Init(void);   // 初始化USART1串口（PA9-TX, PA10-RX, 115200-8N1）
static void MX_TIM2_Init(void);          // 初始化TIM2定时器（PWM输出：CH1~CH4 → 舵机0~3）
static void MX_TIM3_Init(void);          // 初始化TIM3定时器（PWM输出：CH1 → 舵机4）

// ---- 灵巧手控制核心函数 ----
static void Hand_Init(void);             // 舵机初始化：全部置为张开位置
static void Hand_Tick(uint32_t now_ms);  // 每20ms调用一次：超时检测 + 平滑运动 + PWM更新
static void Hand_SetGrip(float grip, uint32_t now_ms);        // 整体抓握：0.0=全开, 1.0=全闭
static void Hand_SetFingers(const float fingers[HAND_SERVO_COUNT], uint32_t now_ms); // 单指独立控制
static void Hand_WriteServoUs(uint8_t servo_id, uint16_t pulse_us);  // 写入单个舵机PWM脉宽（限幅500~2500us）

// ---- 串口通信与命令解析 ----
static void Hand_ProcessLine(const char *line);   // 解析一行串口命令并执行
static void Hand_UartWrite(const char *text);     // 通过串口发送字符串
static void Hand_UartWriteStatus(void);           // 发送当前5路舵机状态
static bool Hand_RxPop(uint8_t *out);             // 从环形缓冲区取一个字节
static bool Hand_ParseUnitValue(const char **cursor, float *out); // 解析浮点数（0.0~1.0）
static float Hand_Clamp01(float value);           // 将数值限制到[0.0, 1.0]区间
static void Hand_StartUartRx(void);               // 启动USART1单字节中断接收

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  主函数入口 - 灵巧手控制程序
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();   // 初始化HAL库：复位所有外设、初始化Flash接口和SysTick定时器

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();   // 配置系统时钟：HSE(8MHz) → PLL×9 → 72MHz SYSCLK

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();   // GPIO初始化（CubeMX生成，使能GPIOC/GPIOD时钟）
  /* USER CODE BEGIN 2 */
  MX_USART1_UART_Init();   // 初始化USART1（PA9-TX, PA10-RX）
  MX_TIM2_Init();          // 初始化TIM2 PWM（CH1~CH4 → 舵机0~3）
  MX_TIM3_Init();          // 初始化TIM3 PWM（CH1 → 舵机4）
  Hand_Init();             // 舵机初始化：5个舵机全部归位到张开位置
  Hand_StartUartRx();      // 启动串口中断接收（单字节）
  Hand_UartWrite("3D_HAND_READY\r\n");   // 上电就绪信号，通知上位机可以开始发送命令

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static char line[96];            // 串口命令行缓冲区
    static uint8_t line_len = 0U;    // 当前命令行已接收字节数
    static uint32_t next_control_ms = 0U;  // 下次控制循环的时间点
    uint8_t byte;

    // ---- 步骤1：从环形缓冲区逐字节读取，拼接成完整的命令行 ----
    while (Hand_RxPop(&byte))
    {
      if (byte == '\r')
      {
        continue;   // 忽略回车符
      }
      if (byte == '\n')
      {
        // 遇到换行符，一行命令接收完毕，执行命令
        line[line_len] = '\0';
        Hand_ProcessLine(line);
        line_len = 0U;
      }
      else if (line_len < (sizeof(line) - 1U))
      {
        line[line_len++] = (char)byte;   // 普通字符追加到行缓冲区
      }
      else
      {
        line_len = 0U;   // 缓冲区溢出，丢弃该行，重新开始
      }
    }

    // ---- 步骤2：每20ms执行一次舵机控制循环 ----
    uint32_t now_ms = HAL_GetTick();
    if ((int32_t)(now_ms - next_control_ms) >= 0)
    {
      Hand_Tick(now_ms);   // 超时检测 + 平滑运动插值 + PWM更新
      next_control_ms = now_ms + HAND_CONTROL_PERIOD_MS;
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief 系统时钟配置：HSE(8MHz) → PLL×9 → SYSCLK=72MHz, APB1=36MHz, APB2=72MHz
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** 配置振荡器：使用外部高速晶振(HSE) 8MHz，旁路模式（已有外部时钟源） */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;       // HSE旁路模式（外部直接提供时钟）
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;  // HSE不分频
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;            // 保持内部HSI开启作为备用
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;        // 使能PLL
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE; // PLL时钟源选择HSE
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;        // PLL倍频×9 → 8MHz×9=72MHz
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** 配置总线时钟分频：SYSCLK=72M, HCLK=72M, APB1=36M, APB2=72M */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // 系统时钟来源=PLL输出
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         // HCLK = SYSCLK/1 = 72MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;          // APB1 = HCLK/2 = 36MHz（TIM2/3在此总线）
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;          // APB2 = HCLK/1 = 72MHz（USART1在此总线）

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)  // 72MHz需要2个Flash等待周期
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief  初始化USART1串口：PA9(TX,复用推挽) + PA10(RX,上拉输入), 115200-8N1
  * @note   使能USART1中断，优先级为1,0（抢占1，子优先级0）
  */
static void MX_USART1_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 使能GPIOA和USART1时钟
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  // PA9: USART1_TX → 复用推挽输出
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // PA10: USART1_RX → 上拉输入（空闲时保持高电平）
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 配置USART1：115200波特率, 8数据位, 1停止位, 无校验, 无流控
  huart1.Instance = USART1;
  huart1.Init.BaudRate = HAND_UART_BAUDRATE;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;        // 全双工模式
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  // 配置USART1中断优先级并使能
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
  * @brief  配置PWM输出GPIO引脚：PA0,PA1,PA2,PA3（TIM2_CH1~CH4）+ PA6（TIM3_CH1）
  * @note   所有引脚配置为复用推挽输出，低速即可（50Hz PWM）
  */
static void Hand_PWM_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  // PA0(舵机0/拇指), PA1(舵机1/食指), PA2(舵机2/中指), PA3(舵机3/无名指), PA6(舵机4/小指)
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;        // 复用推挽输出
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // 低速（PWM频率仅50Hz，低速足够）
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  初始化TIM2定时器：4通道PWM输出，50Hz，1us分辨率
  * @note   定时器时钟72MHz, 预分频71→计数频率1MHz, 周期19999→20ms(50Hz)
  *         CH1=PA0(舵机0), CH2=PA1(舵机1), CH3=PA2(舵机2), CH4=PA3(舵机3)
  */
static void MX_TIM2_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();
  Hand_PWM_GPIO_Init();   // 先初始化PWM对应的GPIO引脚

  // TIM2时基配置：72MHz/(71+1)=1MHz计数频率，周期(19999+1)=20000us=20ms
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 72U - 1U;                    // 预分频：72分频 → 1MHz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;         // 向上计数模式
  htim2.Init.Period = HAND_PWM_PERIOD_US - 1U;         // 自动重载值：19999 → 20ms周期
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  // PWM输出配置：PWM模式1（CNT<CCR时输出高电平），初始脉宽1000us（张开位置）
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000U;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  // 配置4个PWM通道并立即启动输出
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

/**
  * @brief  初始化TIM3定时器：1通道PWM输出，50Hz，1us分辨率
  * @note   配置同TIM2，仅使用CH1=PA6（舵机4/小指）
  */
static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  Hand_PWM_GPIO_Init();   // 注意：此函数会被TIM2和TIM3各调用一次，重复初始化GPIO无副作用

  // TIM3时基配置：同TIM2，72MHz/(71+1)=1MHz, 周期20000us=20ms
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

  // TIM3仅使用CH1，控制第5个舵机（小指）
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

/**
  * @brief  舵机初始化：将所有舵机的当前值和目标值设为张开位置，并写入PWM
  */
static void Hand_Init(void)
{
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    servo_current_us[i] = servo_open_us[i];   // 当前位置 = 张开位置
    servo_target_us[i] = servo_open_us[i];    // 目标位置 = 张开位置
    Hand_WriteServoUs(i, servo_current_us[i]); // 立即输出PWM
  }
  last_command_ms = HAL_GetTick();   // 记录初始时间戳，避免启动时立即触发超时
}

/**
  * @brief  舵机控制周期任务（每20ms调用一次）
  * @param  now_ms: 当前系统时间戳（ms）
  * @note   功能1：命令超时检测 → 超过500ms无新命令则自动张开（安全保护）
  *         功能2：平滑运动插值 → 每20ms最多改变10us脉宽，避免舵机突变抖动
  *         功能3：将插值后的PWM值写入硬件比较寄存器
  */
static void Hand_Tick(uint32_t now_ms)
{
  // ---- 超时保护：超过500ms未收到命令，自动回到张开位置 ----
  if ((now_ms - last_command_ms) > HAND_COMMAND_TIMEOUT_MS)
  {
    for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
    {
      servo_target_us[i] = servo_open_us[i];
    }
  }

  // ---- 平滑运动：逐步逼近目标位置，每次最多变化10us ----
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    if (servo_current_us[i] < servo_target_us[i])
    {
      // 当前值小于目标值 → 增大脉宽（闭合方向）
      uint16_t delta = servo_target_us[i] - servo_current_us[i];
      servo_current_us[i] += (delta > 10U) ? 10U : delta;  // 步进限制：最多+10us
    }
    else if (servo_current_us[i] > servo_target_us[i])
    {
      // 当前值大于目标值 → 减小脉宽（张开方向）
      uint16_t delta = servo_current_us[i] - servo_target_us[i];
      servo_current_us[i] -= (delta > 10U) ? 10U : delta;  // 步进限制：最多-10us
    }

    // 将计算后的PWM值写入硬件
    Hand_WriteServoUs(i, servo_current_us[i]);
  }
}

/**
  * @brief  设置整体抓握比例（5指同步）
  * @param  grip: 抓握比例 0.0=全张开, 1.0=全闭合
  * @param  now_ms: 当前时间戳，用于刷新超时计时
  * @note   自动将grip钳位到[0.0, 1.0]，线性映射到每根手指的开/闭脉宽之间
  */
static void Hand_SetGrip(float grip, uint32_t now_ms)
{
  grip = Hand_Clamp01(grip);   // 钳位到[0, 1]
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    // 线性插值：target = open + (close - open) * grip
    servo_target_us[i] = (uint16_t)((float)servo_open_us[i] +
                         ((float)servo_close_us[i] - (float)servo_open_us[i]) * grip);
  }
  last_command_ms = now_ms;   // 刷新命令时间戳，防止超时
}

/**
  * @brief  独立设置每根手指的闭合比例
  * @param  fingers: 长度为5的数组，每根手指的闭合比例[0.0, 1.0]
  * @param  now_ms: 当前时间戳
  * @note   索引对应：0=拇指, 1=食指, 2=中指, 3=无名指, 4=小指
  */
static void Hand_SetFingers(const float fingers[HAND_SERVO_COUNT], uint32_t now_ms)
{
  for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
  {
    float value = Hand_Clamp01(fingers[i]);   // 每个值独立钳位
    servo_target_us[i] = (uint16_t)((float)servo_open_us[i] +
                         ((float)servo_close_us[i] - (float)servo_open_us[i]) * value);
  }
  last_command_ms = now_ms;
}

/**
  * @brief  将PWM脉宽写入指定舵机的硬件比较寄存器
  * @param  servo_id: 舵机编号（0~4）
  * @param  pulse_us: PWM高电平脉宽（us），会被钳位到[500, 2500]安全范围
  * @note   引脚映射：0→PA0(TIM2_CH1), 1→PA1(TIM2_CH2), 2→PA2(TIM2_CH3),
  *                   3→PA3(TIM2_CH4), 4→PA6(TIM3_CH1)
  */
static void Hand_WriteServoUs(uint8_t servo_id, uint16_t pulse_us)
{
  // 脉宽安全限幅：SG90舵机安全范围500~2500us
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
    case 0: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us); break;  // 拇指 PA0
    case 1: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse_us); break;  // 食指 PA1
    case 2: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse_us); break;  // 中指 PA2
    case 3: __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pulse_us); break;  // 无名指 PA3
    case 4: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse_us); break;  // 小指 PA6
    default: break;
  }
}

/**
  * @brief  解析并执行一行串口命令
  * @param  line: 以'\0'结尾的命令字符串（不含换行符）
  * @note   支持的命令格式：
  *         - PING         → 心跳测试，回复PONG
  *         - OPEN / O     → 五指全开
  *         - CLOSE / C    → 五指全闭
  *         - STATUS / S   → 查询当前PWM状态
  *         - G,0.5        → 整体抓握比例（0.0~1.0）
  *         - F,0.2,0.5,0.8,0.3,0.6 → 五指独立控制
  */
static void Hand_ProcessLine(const char *line)
{
  if (line == NULL || line[0] == '\0')
  {
    return;   // 空行忽略
  }

  // ---- 命令：OPEN / O → 五指全开 ----
  if (strcmp(line, "OPEN") == 0 || strcmp(line, "O") == 0)
  {
    Hand_SetGrip(0.0f, HAL_GetTick());   // grip=0.0 即全部张开
    Hand_UartWrite("OK,OPEN\r\n");
    return;
  }

  // ---- 命令：CLOSE / C → 五指全闭 ----
  if (strcmp(line, "CLOSE") == 0 || strcmp(line, "C") == 0)
  {
    Hand_SetGrip(1.0f, HAL_GetTick());   // grip=1.0 即全部闭合
    Hand_UartWrite("OK,CLOSE\r\n");
    return;
  }

  // ---- 命令：PING → 心跳测试 ----
  if (strcmp(line, "PING") == 0)
  {
    Hand_UartWrite("PONG\r\n");
    return;
  }

  // ---- 命令：STATUS / S → 查询状态 ----
  if (strcmp(line, "STATUS") == 0 || strcmp(line, "S") == 0)
  {
    Hand_UartWriteStatus();   // 返回5路当前值和目标值
    return;
  }

  // ---- 命令：G,<value> → 整体抓握比例控制 ----
  if (line[0] == 'G' && line[1] == ',')
  {
    const char *cursor = line + 2;   // 跳过"G,"
    float grip = 0.0f;
    if (Hand_ParseUnitValue(&cursor, &grip) && *cursor == '\0')
    {
      Hand_SetGrip(grip, HAL_GetTick());
      Hand_UartWrite("OK,G\r\n");
      return;
    }
    Hand_UartWrite("ERR,G\r\n");   // 解析失败
    return;
  }

  // ---- 命令：F,<v0>,<v1>,<v2>,<v3>,<v4> → 五指独立控制 ----
  if (line[0] == 'F' && line[1] == ',')
  {
    const char *cursor = line + 2;   // 跳过"F,"
    float fingers[HAND_SERVO_COUNT];
    for (uint8_t i = 0U; i < HAND_SERVO_COUNT; ++i)
    {
      if (!Hand_ParseUnitValue(&cursor, &fingers[i]))
      {
        return;   // 解析失败，静默忽略
      }
      if (i + 1U < HAND_SERVO_COUNT)
      {
        if (*cursor != ',')
        {
          return;   // 分隔符不是逗号，格式错误
        }
        cursor++;   // 跳过逗号
      }
    }
    if (*cursor != '\0')
    {
      Hand_UartWrite("ERR,F\r\n");   // 末尾有多余字符
      return;
    }
    Hand_SetFingers(fingers, HAL_GetTick());
    Hand_UartWrite("OK,F\r\n");
    return;
  }

  // ---- 未知命令 ----
  Hand_UartWrite("ERR,UNKNOWN\r\n");
}

/**
  * @brief  通过USART1发送字符串（阻塞模式，超时50ms）
  * @param  text: 要发送的C字符串
  */
static void Hand_UartWrite(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 50U);
}

/**
  * @brief  发送舵机状态报告：当前脉宽和目标脉宽（共10个值，逗号分隔）
  * @note   格式：STATUS,cur0,cur1,cur2,cur3,cur4,tgt0,tgt1,tgt2,tgt3,tgt4\r\n
  */
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

/**
  * @brief  从环形缓冲区取出一个字节（非阻塞）
  * @param  out: 输出参数，存放取出的字节
  * @return true=成功取出, false=缓冲区为空
  * @note   只读rx_tail指针，由主循环调用；rx_head由中断写入，无需关中断
  */
static bool Hand_RxPop(uint8_t *out)
{
  if (rx_tail == rx_head)
  {
    return false;   // 缓冲区空
  }
  *out = rx_ring[rx_tail];
  rx_tail = (uint16_t)((rx_tail + 1U) % HAND_RX_RING_SIZE);  // 环形递增
  return true;
}

/**
  * @brief  解析[0.0, 1.0]范围的浮点数（支持整数、小数两种格式）
  * @param  cursor: 指向字符串指针的指针，解析后自动前进
  * @param  out: 输出解析后的浮点数值（已钳位到[0.0, 1.0]）
  * @return true=解析成功, false=没有有效数字
  * @note   示例："0.5"→0.5, "1"→1.0, "0.25"→0.25
  *         小数部分最多解析6位（防止frac_scale溢出）
  */
static bool Hand_ParseUnitValue(const char **cursor, float *out)
{
  uint32_t whole = 0U;        // 整数部分
  uint32_t frac = 0U;         // 小数部分
  uint32_t frac_scale = 1U;   // 小数位数权重（10, 100, 1000, ...）
  bool has_digit = false;     // 是否至少解析到一个数字
  const char *p = *cursor;

  // 解析整数部分
  while (*p >= '0' && *p <= '9')
  {
    has_digit = true;
    whole = whole * 10U + (uint32_t)(*p - '0');
    p++;
  }

  // 解析小数部分（如果有小数点）
  if (*p == '.')
  {
    p++;
    while (*p >= '0' && *p <= '9')
    {
      has_digit = true;
      if (frac_scale < 1000000U)   // 限制最多6位小数，防止溢出
      {
        frac = frac * 10U + (uint32_t)(*p - '0');
        frac_scale *= 10U;
      }
      p++;
    }
  }

  if (!has_digit)
  {
    return false;   // 没有有效数字
  }

  // 合成浮点数并钳位到[0, 1]
  *out = Hand_Clamp01((float)whole + ((float)frac / (float)frac_scale));
  *cursor = p;   // 更新游标位置
  return true;
}

/**
  * @brief  将浮点数钳位到 [0.0, 1.0] 区间
  */
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

/**
  * @brief  启动USART1单字节中断接收（每次中断只收1字节，存入环形缓冲区后重新启动）
  */
static void Hand_StartUartRx(void)
{
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U);
}

/**
  * @brief  USART1接收完成中断回调
  * @note   每收到1个字节触发一次，写入环形缓冲区后重新启动接收
  *         如果缓冲区满（head+1==tail），则丢弃该字节（静默丢弃策略）
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    uint16_t next_head = (uint16_t)((rx_head + 1U) % HAND_RX_RING_SIZE);
    if (next_head != rx_tail)   // 缓冲区未满，存入
    {
      rx_ring[rx_head] = rx_byte;
      rx_head = next_head;
    }
    // 否则缓冲区满，丢弃该字节（防止覆盖未处理的数据）
    Hand_StartUartRx();   // 重新启动中断接收
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
/**
  * @brief  错误处理函数：关闭全局中断后进入死循环
  * @note   任何HAL初始化失败都会调用此函数，实际产品中可加入LED闪烁报警
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();   // 关闭全局中断
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
