/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
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

/* Define to prevent recursive inclusion -------------------------------------*/
/* 防止头文件重复包含 */
#ifndef __STM32F1xx_IT_H
#define __STM32F1xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
// Cortex-M3 系统异常处理函数
void NMI_Handler(void);           // 不可屏蔽中断
void HardFault_Handler(void);     // 硬件错误
void MemManage_Handler(void);     // 内存管理错误
void BusFault_Handler(void);      // 总线错误
void UsageFault_Handler(void);    // 未定义指令/非法状态
void SVC_Handler(void);           // 系统服务调用
void DebugMon_Handler(void);      // 调试监视器
void PendSV_Handler(void);        // 可挂起系统服务
void SysTick_Handler(void);       // 系统滴答定时器（HAL时基）
/* USER CODE BEGIN EFP */
void USART1_IRQHandler(void);     // USART1中断处理（HAL库内部调用回调）

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_IT_H */
