/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
	init_s,
	day_set_s,
	month_set_s,
	yearhund_set_s,
	yearunit_set_s,
	write_init_date_s,
	update_DIL_s,
	default_s
} state_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TIM2_INTERRUPT_ENABLE(X) (TIM2->DIER = X)
#define LONG_CLICK 2
#define SHORT_CLICK 1

// Segment display definitions
#define DIG1_PIN GPIO_PIN_7
#define DIG2_PIN GPIO_PIN_8
#define DIG3_PIN GPIO_PIN_0
#define DIG4_PIN GPIO_PIN_1
#define DIG5_PIN GPIO_PIN_6

#define DIG1_PORT GPIOA
#define DIG2_PORT GPIOA
#define DIG3_PORT GPIOC
#define DIG4_PORT GPIOC
#define DIG5_PORT GPIOB

#define ASEG_PIN GPIO_PIN_0
#define BSEG_PIN GPIO_PIN_1
#define CSEG_PIN GPIO_PIN_11
#define DSEG_PIN GPIO_PIN_12
#define ESEG_PIN GPIO_PIN_4
#define FSEG_PIN GPIO_PIN_5
#define GSEG_PIN GPIO_PIN_6

#define SEGMENT_PORT GPIOA


#define LIGHT_0 (ASEG_PIN | BSEG_PIN | CSEG_PIN | DSEG_PIN | ESEG_PIN | FSEG_PIN)
#define LIGHT_1 (BSEG_PIN | CSEG_PIN)
#define LIGHT_2 (ASEG_PIN | BSEG_PIN | DSEG_PIN | ESEG_PIN | GSEG_PIN)
#define LIGHT_3 (ASEG_PIN | BSEG_PIN | CSEG_PIN | DSEG_PIN | GSEG_PIN)
#define LIGHT_4 (BSEG_PIN | CSEG_PIN | FSEG_PIN | GSEG_PIN)
#define LIGHT_5 (ASEG_PIN | CSEG_PIN | DSEG_PIN | FSEG_PIN | GSEG_PIN)
#define LIGHT_6 (ASEG_PIN | CSEG_PIN | DSEG_PIN | ESEG_PIN | FSEG_PIN | GSEG_PIN)
#define LIGHT_7 (ASEG_PIN | BSEG_PIN | CSEG_PIN)
#define LIGHT_8 (ASEG_PIN | BSEG_PIN | CSEG_PIN | DSEG_PIN | ESEG_PIN | FSEG_PIN | GSEG_PIN)
#define LIGHT_9 (ASEG_PIN | BSEG_PIN | CSEG_PIN | DSEG_PIN | FSEG_PIN | GSEG_PIN)

//#define clk40khz
#define debug_RTC
#define debug_disp
#define hardware_debugger

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
static void light_digit(uint16_t digit, uint16_t num);
static uint16_t calc_DIL(RTC_DateTypeDef *date, RTC_DateTypeDef *meetup_date);
static void Configure_Sleep_Mode(uint16_t enable);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint16_t debounce_val = 0;
uint16_t sleep_mode_counter = 0;
uint16_t SLEEP_MODE_ENABLE_FLAG = 0;
uint16_t ENTER_SLEEPMODE_FLAG = 0;
uint16_t click = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	sleep_mode_counter = 0;
	ENTER_SLEEPMODE_FLAG = 0;
	Configure_Sleep_Mode(0);
	return;
}

static void Configure_Sleep_Mode(uint16_t enable)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if (enable)
	{
		/*Configure GPIO pin : PC13 */
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);
		GPIO_InitStruct.Pin = GPIO_PIN_8;
		GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
		/* EXTI interrupt init*/
		HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

		EXTI->IMR |= EXTI_IMR_IM13;
		EXTI->EMR |= EXTI_EMR_EM13;

		TIM2_INTERRUPT_ENABLE(0);
		HAL_TIM_Base_Stop(&htim2);
		ENTER_SLEEPMODE_FLAG = 1;
		// SCB->SCR = SCB_SCR_SLEEPONEXIT_Pos;
		return;
	}
	SLEEP_MODE_ENABLE_FLAG = 0;
	/*Configure GPIO pin : PC13 */
	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
	TIM2_INTERRUPT_ENABLE(1);
	HAL_TIM_Base_Start(&htim2);
	return;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (!(GPIOC->IDR & GPIO_PIN_13))
	{
		if (debounce_val > 600)
		{
			click = 2;
			debounce_val = 0;
			return;
		}
		debounce_val++;
		return;
	}
	if (debounce_val < 10)
	{
		if (SLEEP_MODE_ENABLE_FLAG)
			sleep_mode_counter++;
		if (sleep_mode_counter > 7000)
		{
			Configure_Sleep_Mode(1);
		}
		debounce_val = 0;
		return;
	}
	click = 1;
	debounce_val = 0;
	return;
}

static uint16_t countLeapYears(uint8_t y, uint8_t m)
{
	uint16_t years = y;
	// Check if the current year needs to be
	//  considered for the count of leap years
	// or not
	if (m <= 2)
		years--;
	// An year is a leap year if it
	// is a multiple of 4,
	// multiple of 400 and not a
	// multiple of 100.
	return years / 4 - years / 100 + years / 400;
}

static uint16_t calc_DIL(RTC_DateTypeDef *date, RTC_DateTypeDef *meetup_date)
{
	const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30,
									31, 31, 30, 31, 30, 31};
	// COUNT TOTAL NUMBER OF DAYS
	// BEFORE FIRST DATE 'dt1'
	// initialize count using years and day
	uint16_t n1 = date->Year * 365 + date->Date;
	// Add days for months in given date
	for (int i = 0; i < date->Month - 1; i++)
		n1 += month_days[i];
	// Since every leap year is of 366 days,
	// Add a day for every leap year
	n1 += countLeapYears(date->Year, date->Month);
	// SIMILARLY, COUNT TOTAL NUMBER OF
	// DAYS BEFORE 'dt2'
	uint16_t n2 = meetup_date->Year * 365 + meetup_date->Date;
	for (int i = 0; i < meetup_date->Month - 1; i++)
		n2 += month_days[i];
	n2 += countLeapYears(meetup_date->Year, meetup_date->Month);
	// return difference between two counts
	return (n1 - n2);
}

static void light_digit(uint16_t digit, uint16_t num)
{
	// Pin PA2, PA3 is connected to uart2 4 hardware debugger
	// Pins are remapped to PC0, PC1
	#ifdef hardware_debugger
	// Turn off all digits
	HAL_GPIO_WritePin(DIG1_PORT, DIG1_PIN, 1);
	HAL_GPIO_WritePin(DIG2_PORT, DIG2_PIN, 1);
	HAL_GPIO_WritePin(DIG3_PORT, DIG3_PIN, 1);
	HAL_GPIO_WritePin(DIG4_PORT, DIG4_PIN, 1);
	HAL_GPIO_WritePin(DIG5_PORT, DIG5_PIN, 1);
	switch (digit)
	{
		case 1: HAL_GPIO_WritePin(DIG1_PORT, DIG1_PIN, 0); break;
		case 2: HAL_GPIO_WritePin(DIG2_PORT, DIG2_PIN, 0); break;
		case 3: HAL_GPIO_WritePin(DIG3_PORT, DIG3_PIN, 0); break;
		case 4: HAL_GPIO_WritePin(DIG4_PORT, DIG4_PIN, 0); break;
		default: HAL_GPIO_WritePin(DIG5_PORT, DIG5_PIN, 0); break;
	}
	// Turn off all segments
	HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_8, 0);
	switch (num)
	{
		case 0:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_0, 1); break;
		case 1:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_1, 1); break;
		case 2:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_2, 1); break;
		case 3:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_3, 1); break;
		case 4:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_4, 1); break;
		case 5:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_5, 1); break;
		case 6:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_6, 1); break;
		case 7:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_7, 1); break;
		case 8:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_8, 1); break;
		case 9:		HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_9, 1); break;
		case 10:	HAL_GPIO_WritePin(SEGMENT_PORT, 0, 1); break;
		default:	HAL_GPIO_WritePin(SEGMENT_PORT, LIGHT_0, 1); break;
	}
	#else

	// Turn off all digits
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, 1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_13 | GPIO_PIN_14, 1);
	switch (digit)
	{
	case 1:		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, 0); break;
	case 2:		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 0); break;
	case 3:		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, 0); break;
	case 4:		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_14, 0); break;
	default:	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, 0); break;
	}
	HAL_GPIO_WritePin(GPIOA, 0b11111111, 0);
	switch (num)
	{
	case 0:		HAL_GPIO_WritePin(GPIOA, 0b1111011, 1); break;
	case 1:		HAL_GPIO_WritePin(GPIOA, 0b0001010, 1); break;
	case 2:		HAL_GPIO_WritePin(GPIOA, 0b1011101, 1); break;
	case 3:		HAL_GPIO_WritePin(GPIOA, 0b0011111, 1); break;
	case 4:		HAL_GPIO_WritePin(GPIOA, 0b0101110, 1); break;
	case 5:		HAL_GPIO_WritePin(GPIOA, 0b0110111, 1); break;
	case 6:		HAL_GPIO_WritePin(GPIOA, 0b1110111, 1); break;
	case 7:		HAL_GPIO_WritePin(GPIOA, 0b0011010, 1); break;
	case 8:		HAL_GPIO_WritePin(GPIOA, 0b1111111, 1); break;
	case 9:		HAL_GPIO_WritePin(GPIOA, 0b0111111, 1); break;
	case 10:	HAL_GPIO_WritePin(GPIOA, 0b0000000, 1); break;
	default:	HAL_GPIO_WritePin(GPIOA, 0b1111011, 1); break;
	}
	#endif
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
  MX_GPIO_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	// HAL_NVIC_EnableIRQ(TIM2_IRQn);
	TIM2_INTERRUPT_ENABLE(1);
	HAL_TIM_Base_Start(&htim2);

	// HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_2);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

	RTC_DateTypeDef date = {0};
	RTC_DateTypeDef meetup_date = {0};

	state_t cur_state = init_s;
	uint16_t digits[5];

	uint16_t FLASH_FLAG = 0;
	uint16_t GET_MEETUP_DATE_FLAG = 0;
	uint16_t year = 1940;
	uint16_t DIL = 0;

#ifdef debug_disp
	while (1)
	{
		for (int i = 0; i < 5; i++)
		{
			light_digit(i+1, i+5);
			HAL_Delay(1000);
		}
	}
#endif

#ifdef clk40khz
	uint16_t init_DIL = 0;
#endif

#ifdef debug_RTC
	RTC_TimeTypeDef time = {0};
#endif

	Configure_Sleep_Mode(0);
	while (1)
	{

#ifdef debug_RTC

		HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

		digits[4] = time.SubSeconds % 10;
		digits[3] = time.SubSeconds / 10;
		digits[2] = time.Seconds % 10;
		digits[1] = time.Seconds / 10;

		for (int i = 0; i < 5; i++)
		{
			light_digit(i, digits[i]);
			HAL_Delay(1);
		}
		continue;
#endif

		for (int i = 0; i < 5; i++)
			digits[i] = 10;

		switch (cur_state)
		{
		case init_s:
			for (int i = 0; i < 5; i++)
				digits[i] = 8;
			if (click == LONG_CLICK)
			{
				cur_state = day_set_s;
				click = 0;
			}
			break;
		case day_set_s:
			switch (click)
			{
			case SHORT_CLICK:
				date.Date++;
				if (date.Date > 31)
					date.Date = 1;
				click = 0;
				break;
			case LONG_CLICK:
				if (GET_MEETUP_DATE_FLAG)
					meetup_date.Date = date.Date;
				cur_state = month_set_s;
				click = 0;
				FLASH_FLAG = 1;
				break;
			}
			digits[4] = date.Date % 10;
			digits[3] = date.Date / 10;
			break;
		case month_set_s:
			switch (click)
			{
			case SHORT_CLICK:
				date.Month++;
				if (date.Month > 12)
					date.Month = 1;
				click = 0;
				break;
			case LONG_CLICK:
				if (GET_MEETUP_DATE_FLAG)
					meetup_date.Month = date.Month;
				cur_state = yearhund_set_s;
				click = 0;
				FLASH_FLAG = 1;
				break;
			}
			digits[4] = date.Month % 10;
			digits[3] = date.Month / 10;
			break;
		case yearhund_set_s:
			year = 1940 + date.Year;
			switch (click)
			{
			case SHORT_CLICK:
				date.Year += 10;
				if (date.Year > 240)
					date.Year = 0;
				click = 0;
				break;
			case LONG_CLICK:
				FLASH_FLAG = 1;
				cur_state = yearunit_set_s;
				date.Year = 0;
				click = 0;
				break;
			}
			digits[4] = year % 10;
			digits[3] = (year % 100) / 10;
			digits[2] = (year % 1000) / 100;
			digits[1] = (year) / 1000;
			break;
		case yearunit_set_s:
			switch (click)
			{
			case SHORT_CLICK:
				date.Year += 1;
				if (date.Year > 9)
					date.Year = 0;
				click = 0;
				break;
			case LONG_CLICK:
				FLASH_FLAG = 1;
				cur_state = write_init_date_s;
				year += date.Year;
				date.Year = year - 1940;
				if (GET_MEETUP_DATE_FLAG)
					meetup_date.Year = date.Year;
				click = 0;
				break;
			}
			digits[4] = date.Year % 10;
			digits[3] = (year % 100) / 10;
			digits[2] = (year % 1000) / 100;
			digits[1] = (year) / 1000;
			break;
		case write_init_date_s:
			if (GET_MEETUP_DATE_FLAG)
			{
				GET_MEETUP_DATE_FLAG = 0;
				cur_state = update_DIL_s;
				break;
			}
			// Initialize RTC
			HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
			cur_state = update_DIL_s;
			break;
		case update_DIL_s:
			HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
			DIL = calc_DIL(&date, &meetup_date);

#ifdef clk40khz
			if (init_DIL)
			{
				DIL = init_DIL + (uint16_t)round(32.768f / 40.0f * calc_DIL(&date, &meetup_date));
			}
			else
			{
				init_DIL = calc_DIL(&date, &meetup_date);
				meetup_date = date;
			}
#endif

			cur_state = default_s;
			break;
		case default_s:
			if (click == LONG_CLICK)
			{
				click = 0;
				FLASH_FLAG = 1;
				GET_MEETUP_DATE_FLAG = 1;
				date.Date = 0;
				date.Month = 0;
				date.Year = 0;
				SLEEP_MODE_ENABLE_FLAG = 0;
				cur_state = day_set_s;

#ifdef clk40khz
				init_DIL = 0;
#endif
			}
			SLEEP_MODE_ENABLE_FLAG = 1;
			digits[4] = DIL % 10;
			digits[3] = (DIL % 100) / 10;
			digits[2] = (DIL % 1000) / 100;
			digits[1] = (DIL % 10000) / 1000;
			digits[0] = (DIL) / 10000;
			break;
		}

		if (FLASH_FLAG)
		{
			GPIOA->ODR = 0x3ff;
			GPIOB->ODR = 0;
			HAL_Delay(500);
			FLASH_FLAG = 0;
		}

		for (int i = 0; i < 5; i++)
		{
			light_digit(i, digits[i]);
			HAL_Delay(1);
		}

		// Weird shit sleep mode config
		if (ENTER_SLEEPMODE_FLAG)
		{
			GPIOB->ODR = 0;
			GPIOA->ODR = 0;
			SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
			__disable_irq();
			__SEV();
			__WFE();
			__WFE();
			SystemClock_Config();
			SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
			__enable_irq();
			cur_state = update_DIL_s;
		}

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */
  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 100;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 2000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, DIG3_Pin|DIG4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, A_Pin|B_Pin|E_Pin|F_Pin
                          |G_Pin|DIG1_Pin|DIG2_Pin|C_Pin
                          |D_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD2_Pin|DIG5_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DIG3_Pin DIG4_Pin */
  GPIO_InitStruct.Pin = DIG3_Pin|DIG4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : A_Pin B_Pin E_Pin F_Pin
                           G_Pin DIG1_Pin DIG2_Pin C_Pin
                           D_Pin */
  GPIO_InitStruct.Pin = A_Pin|B_Pin|E_Pin|F_Pin
                          |G_Pin|DIG1_Pin|DIG2_Pin|C_Pin
                          |D_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin DIG5_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|DIG5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : External_button_Pin */
  GPIO_InitStruct.Pin = External_button_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(External_button_GPIO_Port, &GPIO_InitStruct);

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

