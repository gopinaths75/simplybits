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
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_uart.h"
#include "stdio.h"
#include "st7735.h"
#include "fonts.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t dayofweek;
    uint8_t dayofmonth;
    uint8_t month;
    uint8_t year;
} RTC_Time;

RTC_Time myTime;
#define DS3231_ADDRESS 0xD0

#define DHT22_GPIO_Port GPIOA
#define DHT22_Pin GPIO_PIN_1

uint8_t decToBcd(uint8_t val) {
  return ( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
uint8_t bcdToDec(uint8_t val) {
  return ( (val/16*10) + (val%16) );
}

void DS3231_SetTime(uint8_t sec, uint8_t min, uint8_t hour, uint8_t dow, uint8_t dom, uint8_t month, uint8_t year) {
    uint8_t set_time[7];
    set_time[0] = decToBcd(sec);
    set_time[1] = decToBcd(min);
    set_time[2] = decToBcd(hour); // Assumes 24-hour mode
    set_time[3] = decToBcd(dow);
    set_time[4] = decToBcd(dom);
    set_time[5] = decToBcd(month);
    set_time[6] = decToBcd(year);

    // Write 7 bytes starting at register 0x00 (Seconds register)
    HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, 0x00, I2C_MEMADD_SIZE_8BIT, set_time, 7, 1000);
}

void DS3231_GetTime(RTC_Time *time) {
    uint8_t get_time[7];
    
    // Read 7 bytes starting at register 0x00
    HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, 0x00, I2C_MEMADD_SIZE_8BIT, get_time, 7, 1000);

    time->seconds    = bcdToDec(get_time[0]);
    time->minutes    = bcdToDec(get_time[1]);
    time->hours      = bcdToDec(get_time[2] & 0x3F); // Masking for 24-hour mode
    time->dayofweek  = bcdToDec(get_time[3]);
    time->dayofmonth = bcdToDec(get_time[4]);
    time->month      = bcdToDec(get_time[5] & 0x1F); // Masking out century bit
    time->year       = bcdToDec(get_time[6]);
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// int _write(int file, char *ptr, int len)
// {
//     /* Transmit the buffer over USART1 with a generous 100ms timeout per block */
//     HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, 100);
//     return len;
// }

// For Temp and Humid Sensor

void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < us);
}
void Set_Pin_Output (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // Open drain allows easier line release
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

uint8_t DHT22_Start(void) {
    uint8_t response = 0;
    Set_Pin_Output(DHT22_GPIO_Port, DHT22_Pin);
    // 1. MCU pulls the data line low for 18 milliseconds
    HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_RESET);
    HAL_Delay(18); 
    
    // 2. MCU pulls high / releases the bus for 30 microseconds
    HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
    delay_us(30);
    
    Set_Pin_Input(DHT22_GPIO_Port, DHT22_Pin);
    // 3. Check if DHT22 response is low (80us pulse start)
    if (!(HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin))) {
        delay_us(80);
        
        // 4. Check if DHT22 response goes high (80us pulse high)
        if ((HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin))) {
            response = 1; // Handshake successful
        }
        else response = -1;
    }
    
    // Wait for the high response pulse to finish before reading bits
    uint32_t timeout = 0;
    while ((HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin)) && timeout < 100) {
        delay_us(1);
        timeout++;
    }
    
    return response;
}

uint8_t DHT22_Read_Byte(void) {
    uint8_t i, result = 0;
    Set_Pin_Input(DHT22_GPIO_Port, DHT22_Pin);
    for (i = 0; i < 8; i++) {
        // Wait for the 50us LOW period to pass
        while (!(HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin))); 
        
        // Line is now HIGH. Wait 40us to sample the bit duration
        delay_us(40); 
        
        if (!(HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin))) {
            result &= ~(1 << (7 - i));   // If line is LOW now, it was a '0' bit
        } else {
            result |= (1 << (7 - i));    // If line is still HIGH, it's a '1' bit
            while ((HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin))); // Wait out the rest of the '1' bit
        }
    }
    return result;
}

uint8_t DHT22_Read_Data(float *Temperature, float *Humidity) {
    uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2, CheckSum;
    uint16_t SUM, RH, TEMP;

    if (DHT22_Start()) {
        Rh_byte1  = DHT22_Read_Byte();
        Rh_byte2  = DHT22_Read_Byte();
        Temp_byte1 = DHT22_Read_Byte();
        Temp_byte2 = DHT22_Read_Byte();
        CheckSum  = DHT22_Read_Byte();
        
        SUM = Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2;
        
        if (CheckSum == (SUM & 0xFF)) {
            RH = (Rh_byte1 << 8) | Rh_byte2;
            TEMP = (Temp_byte1 << 8) | Temp_byte2;
            
            *Humidity = (float)RH / 10.0;
            
            // Handle negative temperature values (MSB of Temp_byte1 is sign bit)
            if (TEMP & 0x8000) {
                *Temperature = (float)(TEMP & 0x7FFF) / -10.0;
            } else {
                *Temperature = (float)TEMP / 10.0;
            }
            return 1; // Success
        }
    }
    return 0; // Failure/Timeout
}

// for Temp and Humid sensor

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
      char buffer[100]; 
      char buffer1[100];
      char buffer2[100]; 
      char buffer3[100]; 
      char buffer4[100]; 
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
  MX_SPI1_Init();
  ST7735_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  //DS3231_SetTime(0, 5, 15,44 , 2,9, 26);
  HAL_TIM_Base_Start(&htim1); // Start microsecond reference timer
  HAL_Delay(2000); 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  int a=1;
  ST7735_FillScreen(ST7735_RED);
  float temperature = 0.0f;
  float humidity = 0.0f;
  uint8_t success = 0;
  while (1)
  {
    success = DHT22_Read_Data(&temperature, &humidity);
    // HAL_Delay(1000);
     uint32_t current_time = HAL_GetTick();
    // /* USER CODE END WHILE */
    // printf("Hello I am STM32 PlatformIO! %d\r\n",a++);
     int temp_whole = (int)temperature;
        int temp_dec = (int)((temperature - temp_whole) * 10);
        if (temp_dec < 0) temp_dec = -temp_dec; // Handle negative decimals

        int hum_whole = (int)humidity;
        int hum_dec = (int)((humidity - hum_whole) * 10);

    // //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    char *stm = "STM32F4";
    DS3231_GetTime(&myTime);
    snprintf(buffer, sizeof(buffer),"%d:%d:%d" , myTime.hours, myTime.minutes, myTime.seconds);
    snprintf(buffer1, sizeof(buffer1),"%d-%d-%d", myTime.dayofmonth,myTime.month,myTime.year);
    snprintf(buffer3, sizeof(buffer3),"Temp:%d.%dC ",temp_whole, temp_dec);
    snprintf(buffer4, sizeof(buffer4),"Humy:%d.%d%%", hum_whole, hum_dec);
    snprintf(buffer2, sizeof(buffer2),"Success : %d",success);
    // Set cursor and write string to local buffer
    ST7735_WriteString(2, 2, stm, Font_16x26, ST7735_YELLOW, ST7735_RED);
    ST7735_WriteString(2, 30, buffer, Font_11x18, ST7735_BLUE, ST7735_GREEN);
    ST7735_WriteString(2, 52, buffer1, Font_11x18, ST7735_GREEN, ST7735_BLUE);
    ST7735_WriteString(2, 75, buffer2, Font_11x18, ST7735_GREEN, ST7735_BLUE);
    ST7735_WriteString(2, 95, buffer3, Font_11x18, ST7735_GREEN, ST7735_BLUE);
    ST7735_WriteString(2, 115, buffer4, Font_11x18, ST7735_GREEN, ST7735_BLUE);
    HAL_Delay(1000- (HAL_GetTick()-current_time));

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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 95;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
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
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Temp_Hum_GPIO_Port, Temp_Hum_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_RST_Pin|LCD_RS_Pin|LCD_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Temp_Hum_Pin LCD_RST_Pin LCD_RS_Pin LCD_CS_Pin */
  GPIO_InitStruct.Pin = Temp_Hum_Pin|LCD_RST_Pin|LCD_RS_Pin|LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
