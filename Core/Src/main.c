/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 *****************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_customhid.h"
#include "tm_stm32_usb_hid_device.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_Channel 6
#define Axis_Channel 4
#define Trigger_Channel 2
#define Trigger_min 1100
#define Trigger_max 3000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;
extern TM_USB_HIDDEVICE_Gamepad_t Gamepad1;
extern TM_USB_HIDDEVICE_DualShock4_t DS4_1;
extern TM_USB_HIDDEVICE_Status_t TM_USB_HIDDEVICE_INT_Status;

uint8_t buffer[10] = {0};
uint8_t buf[10] = {3, 5, 7, 9, 11, 13, 15, 19};
uint16_t ADC_buffer[ADC_Channel] = {0};
uint8_t axis_value[Axis_Channel] = {0};
uint16_t Trigger_value[Trigger_Channel] = {0};
uint8_t rx_buf[4];
uint8_t tx_buf[4] = {0x55, 0x49, 0x48, 0x79};
int16_t angle, u8angle;
uint8_t init_done = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Key_Scan(void);
void D_PAD(TM_USB_HIDDEVICE_DualShock4_t *p);
void Systick_isr(void);
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
  SysTick_Config(72000000 / 5000); //72Mhz / 50000 = 144000
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USB_DEVICE_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_RESET);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&ADC_buffer, ADC_Channel);
  // HAL_UART_Receive_DMA(&huart3, rx_buf, sizeof(rx_buf));
  TM_USB_HIDEVICE_DualShock4_StructInit(&DS4_1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Key_Scan();
    D_PAD(&DS4_1);
    // HAL_UART_Receive(&huart3, rx_buf, sizeof(rx_buf), 100);
    // HAL_UART_Transmit(&huart1, tx_buf, sizeof(tx_buf), 100);
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    // 方向盘输入，angle值：-5400~5400 -> -127~127
    if (rx_buf[0] == 0x55)
    {
      angle = ((int16_t)rx_buf[2] << 8) + rx_buf[3];
      if (rx_buf[1] == 1)
      {
        angle = -angle;
      }
      // 欧卡�?大一圈半，赛车最�?2700
      u8angle = angle * 127 / 2700 + 127;
      if (u8angle > 0xff)
      {
        u8angle = 0xff;
      }
      else if (u8angle < 0)
      {
        u8angle = 0;
      }
    }
    for (int i = 0; i < Axis_Channel; i++)
    {
      axis_value[i] = (int8_t)(((int16_t)ADC_buffer[i] >> 4));
    }
    // Trigger_max = 3000, Trigger_min = 1200;
    for (int i = 4; i <= 5; i++)
    {
      if (ADC_buffer[i] < Trigger_min)
      {
        Trigger_value[i - 4] = 0;
      }
      else
      {
        Trigger_value[i - 4] = (ADC_buffer[i] - 960) >> 3;
        if (Trigger_value[i - 4] > 0xff)
          Trigger_value[i - 4] = 0xff;
      }
    }
    if (axis_value[1] > 250 || axis_value[1] < 20)
    {
      DS4_1.LeftXAxis = axis_value[1];
    }
    else
    {
      DS4_1.LeftXAxis = (uint8_t)u8angle;
    }
    DS4_1.LeftXAxis = axis_value[1];
    DS4_1.LeftYAxis = axis_value[0];
    DS4_1.RightXAxis = axis_value[3];
    DS4_1.RightYAxis = axis_value[2];

    DS4_1.L2Trigger = (uint8_t)Trigger_value[0];
    DS4_1.R2Trigger = (uint8_t)Trigger_value[1];
    DS4_1.L2 = (DS4_1.L2Trigger > 100) ? 1 : 0; // 可能导致不灵敏？
    DS4_1.R2 = (DS4_1.R2Trigger > 100) ? 1 : 0;
    //		USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, (uint8_t *)&buf, 7);
    //  	TM_USB_HIDDEVICE_GamepadSend(0x01, &Gamepad1);
    init_done ++;
    if(init_done > 101)
    init_done = 101;
    // HAL_Delay(8);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// PB3~6 方向�?
// PB12~15 形状�?
void Key_Scan(void)
{
  DS4_1.Left = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) ? 0 : 1;
  DS4_1.Up = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) ? 0 : 1;
  DS4_1.right = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) ? 0 : 1;
  DS4_1.down = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) ? 0 : 1;

  DS4_1.rectangle = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) ? 0 : 1;
  DS4_1.Triangle = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) ? 0 : 1;
  DS4_1.circle = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) ? 0 : 1;
  DS4_1.cross = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) ? 0 : 1;

  DS4_1.R3 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) ? 0 : 1;
  DS4_1.L3 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) ? 0 : 1;
  DS4_1.R1 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) ? 0 : 1;
  DS4_1.L1 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) ? 0 : 1;

  // DS4_1.option = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) ? 0 : 1;
  // DS4_1.power = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) ? 0 : 1;
}

void D_PAD(TM_USB_HIDDEVICE_DualShock4_t *p)
{
  uint8_t key = 0;
  key = p->Up + (p->right << 1) + (p->down << 2) + (p->Left << 3);
  switch (key)
  {
  case 1: // 0001 N
    p->D_PAD = 0;
    break;
  case 2: // 0010 E
    p->D_PAD = 2;
    break;
  case 3: // 0011 NE
    p->D_PAD = 1;
    break;
  case 4: // 0100 S
    p->D_PAD = 4;
    break;
  case 6: // 0110 SE
    p->D_PAD = 3;
    break;
  case 8: // 1000 W
    p->D_PAD = 6;
    break;
  case 9: // 1001 NW
    p->D_PAD = 7;
    break;
  case 12: // 1100 SW
    p->D_PAD = 5;
    break;
  default:
    p->D_PAD = 8;
    break;
  }
}

void Systick_isr(void)
{
  if (init_done >= 100)
  TM_USB_HIDDEVICE_DualShock4_Send(0x01, &DS4_1);
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
