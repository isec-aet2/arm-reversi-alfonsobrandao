/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * TO DO LIST:
 *			* GOAL 2: DESENHAR O MAPA DE JOGO
 *
 *
 *
 *
 *
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_ts.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	MENU, SINGLEPLAYER, MULTIPLAYER, SCORES, RULES
} states;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TEMP_REFRESH_PERIOD   1000    /* Internal temperature refresh period */
#define MAX_CONVERTED_VALUE   4095    /* Max converted value */
#define AMBIENT_TEMP            25    /* Ambient Temperature */
#define VSENS_AT_AMBIENT_TEMP  760    /* VSENSE value (mv) at ambient temperature */
#define AVG_SLOPE               25    /* Avg_Solpe multiply by 10 */
#define VREF                  3300
#define BOARD_SIZE				 8
#define PLAYER1					 1
#define PLAYER2					 2

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DMA2D_HandleTypeDef hdma2d;

DSI_HandleTypeDef hdsi;

LTDC_HandleTypeDef hltdc;

SD_HandleTypeDef hsd2;

TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;

SDRAM_HandleTypeDef hsdram1;

/* USER CODE BEGIN PV */
bool tsFlag = 0;			//Flag do touchscreen para não marcar vários toques
bool timer_1s = 0;
bool touch_timer = 1;
int touchXvalue = 0, touchYvalue = 0;//Para guardar os valores do toque de X e Y
volatile uint32_t hadc1Value = 0;				//Valor recebido na RSI
long int hadc1ConvertedValue = 0;				//Valor convertido na main
bool timer7Flag = 0;							//Flag do interrupção do Timer6
char tempString[100];						//String a ser enviada para o ecrã

int total_time = 0;
int play_timer = 20;
int num_plays = 0;
bool blue_button = 0;
//NO FIM PARA MOSTRAR O NUMERO DE JOGADAS, SUBTRAIR 2 PARA BATER CERTO
TS_StateTypeDef TS_State;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_DMA2D_Init(void);
static void MX_DSIHOST_DSI_Init(void);
static void MX_FMC_Init(void);
static void MX_LTDC_Init(void);
static void MX_SDMMC2_SD_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */
void LCD_Config_Start(void);
void LCD_Config_Rules(void);
void LCD_Gameplay(void);
void LCD_Update_Timers(void);
void show_temperature(states);
void draw_board(void);
void stamp_play(void);
void init_board(void);
void save_2_sdcard(void);
bool validate_play(int cell_lin, int cell_col, int player);
void update_board(void);
int board[BOARD_SIZE][BOARD_SIZE] = { {0} };//METER PLAYER 1 com 1 e PLAYER 2 com o 2
int valid_plays[BOARD_SIZE][BOARD_SIZE] = { 0 };
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void LCD_Update_Timers(void) {
	char string_turn[30];
	char string_time[30];

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 30);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font16);
	sprintf(string_time, "TOTAL ELAPSED TIME: %d", total_time);
	BSP_LCD_DisplayStringAt(0, 10, (uint8_t *) string_time, CENTER_MODE);

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillRect(0, 460, BSP_LCD_GetXSize(), 30);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font16);
	sprintf(string_turn, "YOUR TURN ENDS IN: %d", play_timer);
	BSP_LCD_DisplayStringAt(0, 460, (uint8_t *) string_turn, CENTER_MODE);
}

void save_2_sdcard(void) {

	unsigned int nBytes = 0;
	char string[20];

	if (f_mount(&SDFatFS, SDPath, 0) != FR_OK)
		Error_Handler();

	if (f_open(&SDFile, "reversi.txt", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
		Error_Handler();

	sprintf(string, "PLAYER1");

	if (f_write(&SDFile, string, strlen(string), &nBytes) != FR_OK)
		Error_Handler();

	f_close(&SDFile);
}

void init_board(void) {

	for (int i = 0; i < BOARD_SIZE; i++) {
		for (int j = 0; j < BOARD_SIZE; j++) {
			board[i][j] = (int) '.';
		}
	}

	board[BOARD_SIZE / 2 - 1][BOARD_SIZE / 2 - 1] = PLAYER1;
	board[BOARD_SIZE / 2][BOARD_SIZE / 2] = PLAYER1;
	board[BOARD_SIZE / 2 - 1][BOARD_SIZE / 2] = PLAYER2;
	board[BOARD_SIZE / 2][BOARD_SIZE / 2 - 1] = PLAYER2;
}

void show_temperature(states state) {

	if (timer7Flag == 1) {

		timer7Flag = 0;

		hadc1ConvertedValue = ((((hadc1Value * VREF) / MAX_CONVERTED_VALUE)
				- VSENS_AT_AMBIENT_TEMP) * 10 / AVG_SLOPE) + AMBIENT_TEMP;
	}
	if (state == SINGLEPLAYER || state == MULTIPLAYER || state == RULES) {
		BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
		BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	} else {
		BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
		BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	}

	sprintf(tempString, "Temp: %ldC", hadc1ConvertedValue);
	BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 20, (uint8_t *) tempString,
			RIGHT_MODE);

}

void draw_board(void) {

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);

	for (int i = 1; i <= 9; i++) {
		BSP_LCD_DrawVLine(150 + (i * 50), 40, 400);
		BSP_LCD_DrawHLine(200, (i * 50) - 10, 400);
	}
}

bool validate_play(int cell_lin, int cell_col, int player) {

	int i = 0, j = 0;
	bool valid = 0, valid_play = 0;

	if (board[cell_lin][cell_col] != '.') {
		return 0;
	}

	//Check left diagonal inferior
	for (i = cell_lin - 1, j = cell_col - 1; i >= 0 && j >= 0; i--, j--) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player) {
				for (int k = cell_lin - 1, x = cell_col - 1; k > i && x > j;
						k--, x--) {
					board[k][x] = player;
					valid_play = 1;
				}
			}
		}
	}

	//Vertical Inferior
	valid = 0;
	for (i = cell_lin - 1, j = cell_col; i >= 0; i--) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player)
				for (int k = cell_lin - 1; k > i; k--) {
					board[k][j] = player;
					valid_play = 1;
				}
		}
	}

	//Inferior Right Diagonal
	valid = 0;
	for (i = cell_lin - 1, j = cell_col + 1; i > 0 && j < BOARD_SIZE;
			i--, j++) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player)
				for (int k = cell_lin - 1, x = cell_col + 1; k > i && x < j;
						k--, x++) {
					board[k][x] = player;
					valid_play = 1;
				}
		}

	}

	//Right Horizontal
	valid = 0;
	for (i = cell_lin, j = cell_col + 1; j < BOARD_SIZE; j++) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player)
				for (int x = cell_col + 1; x < j; x++) {
					board[i][x] = player;
					valid_play = 1;
				}
		}
	}

	//Right Diagonal Superior
	valid = 0;
	for (i = cell_lin + 1, j = cell_col + 1; i < BOARD_SIZE && j < BOARD_SIZE;
			i++, j++) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player)
				for (int k = cell_lin + 1, x = cell_col + 1; k < i && x < j;
						k++, x++) {
					board[k][x] = player;
					valid_play=1;
				}
		}
	}

	//Vertical Superior
	valid = 0;
	for (i = cell_lin + 1, j = cell_col; i < BOARD_SIZE; i++) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player)
				for (int k = cell_lin + 1; k < i; k++) {
					board[k][j] = player;
					valid_play=1;
				}
		}
	}

	//Superior Left Diagonal
	valid = 0;
	for (i = cell_lin + 1, j = cell_col - 1; i < BOARD_SIZE && j >= 0;
			i++, j--) {
		if (!valid) {
			if (board[i][j] == '.')
				break;
			else if (board[i][j] == player)
				break;
			else
				valid = 1;
		} else {
			if (board[i][j] == player)
				for (int k = cell_lin + 1, x = cell_col - 1; k < i && x > j;
						k++, x--) {
					board[k][x] = player;
					valid_play=1;
				}
		}
	}

	//Left Horizontal
	valid = 0;
	for (i = cell_lin, j = cell_col - 1; j >= 0; j--) {
		if (!valid) {
			if (!valid) {
				if (board[i][j] == '.')
					break;
				else if (board[i][j] == player)
					break;
				else
					valid = 1;
			} else {
				if (board[i][j] == player)
					for (int x = cell_col - 1; x > j; x--) {
						board[i][x] = player;
						valid_play=1;
					}
			}
		}
	}
	return valid_play;

}


void stamp_play(void) {

	int player;

	int cell_lin, cell_col;

	if (num_plays % 2 == 0) {
		BSP_LED_On(LED_RED);
		player = PLAYER1;
	}

	else {
		BSP_LED_On(LED_GREEN);
		player = PLAYER2;
	}

	if (play_timer > 0) {

		if (tsFlag == 1) {

			tsFlag = 0;

			if (TS_State.touchX[0] > 200 && TS_State.touchX[0] < 600
					&& TS_State.touchY[0] > 40 && TS_State.touchY[0] < 440) {//SE ESTIVER DENTRO DA BOARD

				cell_lin = (TS_State.touchX[0] - 200) / 50;
				cell_col = (TS_State.touchY[0] - 40) / 50;

				if (validate_play(cell_lin, cell_col, player)) {//SE A JOGADA FOR V�?LIDA
					board[cell_lin][cell_col] = player;

					update_board();

					TS_State.touchX[0] = 0;
					TS_State.touchY[0] = 0;
					BSP_LED_Off(LED_RED);
					BSP_LED_Off(LED_GREEN);

					num_plays++;
					play_timer = 20;
					return;
				} else
					return;
			}
		}
	} else {

		BSP_LED_Off(LED_GREEN);
		BSP_LED_Off(LED_RED);
		num_plays++;
		play_timer = 20;
		return;
	}
}

void update_board(void) {

	// Flipping das peças

	// Apagar Circulos

	BSP_LCD_Clear(LCD_COLOR_WHITE);
	LCD_Update_Timers();
	LCD_Gameplay();

	// Desenhar Circulos

	for (int i = 0; i < BOARD_SIZE; i++) {
		for (int j = 0; j < BOARD_SIZE; j++) {
			int posX = 200 + (i) * 50;
			int posY = 40 + (j) * 50;

			if (board[i][j] == PLAYER1) {
				BSP_LCD_SetTextColor(LCD_COLOR_RED);
				BSP_LCD_FillCircle(posX + 25, posY + 25, 20);
			} else if (board[i][j] == PLAYER2) {
				BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
				BSP_LCD_FillCircle(posX + 25, posY + 25, 20);
			}
		}
	}
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */
	//states state = MENU;

	/* USER CODE END 1 */

	/* Enable I-Cache---------------------------------------------------------*/
	SCB_EnableICache();

	/* Enable D-Cache---------------------------------------------------------*/
	SCB_EnableDCache();

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
	MX_ADC1_Init();
	MX_DMA2D_Init();
	MX_DSIHOST_DSI_Init();
	MX_FMC_Init();
	MX_LTDC_Init();
	MX_SDMMC2_SD_Init();
	MX_TIM6_Init();
	MX_TIM7_Init();
	MX_FATFS_Init();
	/* USER CODE BEGIN 2 */
	BSP_LED_Init(LED_RED);
	BSP_LED_Init(LED_GREEN);
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_TIM_Base_Start_IT(&htim7);
	HAL_ADC_Start_IT(&hadc1);
	BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
	BSP_TS_ITConfig();
	LCD_Config_Start();
	init_board();

	save_2_sdcard();

	states state = MENU;
	char screen_refresh;

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {

		//acrescentar draw_aux_table lá em cima
		show_temperature(state);

		switch (state) {

		case SINGLEPLAYER:
			//SINGLE PLAYER MODE
			if (screen_refresh) {
				while (!timer_1s);
				timer_1s = 0;

				BSP_LCD_Clear(LCD_COLOR_WHITE);

				init_board();

				play_timer = 20;
				total_time = 0;
				LCD_Update_Timers();

				LCD_Gameplay();
				update_board();

				screen_refresh = 0;
			}
			if (timer_1s) {
				LCD_Update_Timers();
				timer_1s = 0;
			}
			stamp_play();

			// Verificar se o tabuleiro está cheio
			// Se sim guradar no SD
			// Mostrar Ecra da Vitoria
			// Clicar para voltar
			break;

		case MULTIPLAYER:
			//MULTIPLAYER MODE
			if (screen_refresh) {
				while (!timer_1s);
				timer_1s = 0;

				BSP_LCD_Clear(LCD_COLOR_WHITE);

				play_timer = 20;
				total_time = 0;
				LCD_Update_Timers();

				LCD_Gameplay();
				update_board();

				screen_refresh = 0;
			}
			if (timer_1s) {
				LCD_Update_Timers();
				timer_1s = 0;
			}
			stamp_play();
			break;

		case SCORES:
			//SCORES
			break;

		case RULES:
			//RULES
			if (screen_refresh) {
				BSP_LCD_Clear(LCD_COLOR_WHITE);
				LCD_Config_Rules();
				screen_refresh = 0;
			}
			if (tsFlag == 1) {
				tsFlag = 0;
				LCD_Config_Start();
				state = MENU;
			}
			break;

		case MENU:

			if (tsFlag == 1 && touchXvalue != 0 && touchXvalue != 0) {

				tsFlag = 0;

				if (touchXvalue <= 200) {
					screen_refresh = 1;
					state = SINGLEPLAYER;
				}

				else if (touchXvalue > 200 && touchXvalue <= 400) {
					screen_refresh = 1;
					state = MULTIPLAYER;
				}

				else if (touchXvalue > 400 && touchXvalue <= 600) {
					screen_refresh = 1;
					state = SCORES;
				}

				else if (touchXvalue > 600 && touchXvalue <= 800) {
					screen_refresh = 1;
					state = RULES;
				}
			}
			break;
		}

		if (blue_button) {
			state = MENU;
			LCD_Config_Start();
			num_plays = 0;
			blue_button = 0;
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
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE()
	;
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 400;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 8;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Activate the Over-Drive mode
	 */
	if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB busses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC
			| RCC_PERIPHCLK_SDMMC2 | RCC_PERIPHCLK_CLK48;
	PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
	PeriphClkInitStruct.PLLSAI.PLLSAIR = 2;
	PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
	PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV2;
	PeriphClkInitStruct.PLLSAIDivQ = 1;
	PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
	PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLL;
	PeriphClkInitStruct.Sdmmc2ClockSelection = RCC_SDMMC2CLKSOURCE_CLK48;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */
	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode = DISABLE;
	hadc1.Init.ContinuousConvMode = ENABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}
	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief DMA2D Initialization Function
 * @param None
 * @retval None
 */
static void MX_DMA2D_Init(void) {

	/* USER CODE BEGIN DMA2D_Init 0 */

	/* USER CODE END DMA2D_Init 0 */

	/* USER CODE BEGIN DMA2D_Init 1 */

	/* USER CODE END DMA2D_Init 1 */
	hdma2d.Instance = DMA2D;
	hdma2d.Init.Mode = DMA2D_M2M;
	hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
	hdma2d.Init.OutputOffset = 0;
	hdma2d.LayerCfg[1].InputOffset = 0;
	hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
	hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
	hdma2d.LayerCfg[1].InputAlpha = 0;
	hdma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
	hdma2d.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;
	if (HAL_DMA2D_Init(&hdma2d) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN DMA2D_Init 2 */

	/* USER CODE END DMA2D_Init 2 */

}

/**
 * @brief DSIHOST Initialization Function
 * @param None
 * @retval None
 */
static void MX_DSIHOST_DSI_Init(void) {

	/* USER CODE BEGIN DSIHOST_Init 0 */

	/* USER CODE END DSIHOST_Init 0 */

	DSI_PLLInitTypeDef PLLInit = { 0 };
	DSI_HOST_TimeoutTypeDef HostTimeouts = { 0 };
	DSI_PHY_TimerTypeDef PhyTimings = { 0 };
	DSI_LPCmdTypeDef LPCmd = { 0 };
	DSI_CmdCfgTypeDef CmdCfg = { 0 };

	/* USER CODE BEGIN DSIHOST_Init 1 */

	/* USER CODE END DSIHOST_Init 1 */
	hdsi.Instance = DSI;
	hdsi.Init.AutomaticClockLaneControl = DSI_AUTO_CLK_LANE_CTRL_DISABLE;
	hdsi.Init.TXEscapeCkdiv = 4;
	hdsi.Init.NumberOfLanes = DSI_ONE_DATA_LANE;
	PLLInit.PLLNDIV = 20;
	PLLInit.PLLIDF = DSI_PLL_IN_DIV1;
	PLLInit.PLLODF = DSI_PLL_OUT_DIV1;
	if (HAL_DSI_Init(&hdsi, &PLLInit) != HAL_OK) {
		Error_Handler();
	}
	HostTimeouts.TimeoutCkdiv = 1;
	HostTimeouts.HighSpeedTransmissionTimeout = 0;
	HostTimeouts.LowPowerReceptionTimeout = 0;
	HostTimeouts.HighSpeedReadTimeout = 0;
	HostTimeouts.LowPowerReadTimeout = 0;
	HostTimeouts.HighSpeedWriteTimeout = 0;
	HostTimeouts.HighSpeedWritePrespMode = DSI_HS_PM_DISABLE;
	HostTimeouts.LowPowerWriteTimeout = 0;
	HostTimeouts.BTATimeout = 0;
	if (HAL_DSI_ConfigHostTimeouts(&hdsi, &HostTimeouts) != HAL_OK) {
		Error_Handler();
	}
	PhyTimings.ClockLaneHS2LPTime = 28;
	PhyTimings.ClockLaneLP2HSTime = 33;
	PhyTimings.DataLaneHS2LPTime = 15;
	PhyTimings.DataLaneLP2HSTime = 25;
	PhyTimings.DataLaneMaxReadTime = 0;
	PhyTimings.StopWaitTime = 0;
	if (HAL_DSI_ConfigPhyTimer(&hdsi, &PhyTimings) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_DSI_ConfigFlowControl(&hdsi, DSI_FLOW_CONTROL_BTA) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_DSI_SetLowPowerRXFilter(&hdsi, 10000) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_DSI_ConfigErrorMonitor(&hdsi, HAL_DSI_ERROR_NONE) != HAL_OK) {
		Error_Handler();
	}
	LPCmd.LPGenShortWriteNoP = DSI_LP_GSW0P_DISABLE;
	LPCmd.LPGenShortWriteOneP = DSI_LP_GSW1P_DISABLE;
	LPCmd.LPGenShortWriteTwoP = DSI_LP_GSW2P_DISABLE;
	LPCmd.LPGenShortReadNoP = DSI_LP_GSR0P_DISABLE;
	LPCmd.LPGenShortReadOneP = DSI_LP_GSR1P_DISABLE;
	LPCmd.LPGenShortReadTwoP = DSI_LP_GSR2P_DISABLE;
	LPCmd.LPGenLongWrite = DSI_LP_GLW_DISABLE;
	LPCmd.LPDcsShortWriteNoP = DSI_LP_DSW0P_DISABLE;
	LPCmd.LPDcsShortWriteOneP = DSI_LP_DSW1P_DISABLE;
	LPCmd.LPDcsShortReadNoP = DSI_LP_DSR0P_DISABLE;
	LPCmd.LPDcsLongWrite = DSI_LP_DLW_DISABLE;
	LPCmd.LPMaxReadPacket = DSI_LP_MRDP_DISABLE;
	LPCmd.AcknowledgeRequest = DSI_ACKNOWLEDGE_DISABLE;
	if (HAL_DSI_ConfigCommand(&hdsi, &LPCmd) != HAL_OK) {
		Error_Handler();
	}
	CmdCfg.VirtualChannelID = 0;
	CmdCfg.ColorCoding = DSI_RGB888;
	CmdCfg.CommandSize = 640;
	CmdCfg.TearingEffectSource = DSI_TE_EXTERNAL;
	CmdCfg.TearingEffectPolarity = DSI_TE_RISING_EDGE;
	CmdCfg.HSPolarity = DSI_HSYNC_ACTIVE_LOW;
	CmdCfg.VSPolarity = DSI_VSYNC_ACTIVE_LOW;
	CmdCfg.DEPolarity = DSI_DATA_ENABLE_ACTIVE_HIGH;
	CmdCfg.VSyncPol = DSI_VSYNC_FALLING;
	CmdCfg.AutomaticRefresh = DSI_AR_ENABLE;
	CmdCfg.TEAcknowledgeRequest = DSI_TE_ACKNOWLEDGE_DISABLE;
	if (HAL_DSI_ConfigAdaptedCommandMode(&hdsi, &CmdCfg) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_DSI_SetGenericVCID(&hdsi, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN DSIHOST_Init 2 */

	/* USER CODE END DSIHOST_Init 2 */

}

/**
 * @brief LTDC Initialization Function
 * @param None
 * @retval None
 */
static void MX_LTDC_Init(void) {

	/* USER CODE BEGIN LTDC_Init 0 */

	/* USER CODE END LTDC_Init 0 */

	LTDC_LayerCfgTypeDef pLayerCfg = { 0 };
	LTDC_LayerCfgTypeDef pLayerCfg1 = { 0 };

	/* USER CODE BEGIN LTDC_Init 1 */

	/* USER CODE END LTDC_Init 1 */
	hltdc.Instance = LTDC;
	hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
	hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
	hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
	hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
	hltdc.Init.HorizontalSync = 7;
	hltdc.Init.VerticalSync = 3;
	hltdc.Init.AccumulatedHBP = 14;
	hltdc.Init.AccumulatedVBP = 5;
	hltdc.Init.AccumulatedActiveW = 654;
	hltdc.Init.AccumulatedActiveH = 485;
	hltdc.Init.TotalWidth = 660;
	hltdc.Init.TotalHeigh = 487;
	hltdc.Init.Backcolor.Blue = 0;
	hltdc.Init.Backcolor.Green = 0;
	hltdc.Init.Backcolor.Red = 0;
	if (HAL_LTDC_Init(&hltdc) != HAL_OK) {
		Error_Handler();
	}
	pLayerCfg.WindowX0 = 0;
	pLayerCfg.WindowX1 = 0;
	pLayerCfg.WindowY0 = 0;
	pLayerCfg.WindowY1 = 0;
	pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
	pLayerCfg.Alpha = 0;
	pLayerCfg.Alpha0 = 0;
	pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
	pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
	pLayerCfg.FBStartAdress = 0;
	pLayerCfg.ImageWidth = 0;
	pLayerCfg.ImageHeight = 0;
	pLayerCfg.Backcolor.Blue = 0;
	pLayerCfg.Backcolor.Green = 0;
	pLayerCfg.Backcolor.Red = 0;
	if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK) {
		Error_Handler();
	}
	pLayerCfg1.WindowX0 = 0;
	pLayerCfg1.WindowX1 = 0;
	pLayerCfg1.WindowY0 = 0;
	pLayerCfg1.WindowY1 = 0;
	pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
	pLayerCfg1.Alpha = 0;
	pLayerCfg1.Alpha0 = 0;
	pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
	pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
	pLayerCfg1.FBStartAdress = 0;
	pLayerCfg1.ImageWidth = 0;
	pLayerCfg1.ImageHeight = 0;
	pLayerCfg1.Backcolor.Blue = 0;
	pLayerCfg1.Backcolor.Green = 0;
	pLayerCfg1.Backcolor.Red = 0;
	if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN LTDC_Init 2 */

	/* USER CODE END LTDC_Init 2 */

}

/**
 * @brief SDMMC2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SDMMC2_SD_Init(void) {

	/* USER CODE BEGIN SDMMC2_Init 0 */

	/* USER CODE END SDMMC2_Init 0 */

	/* USER CODE BEGIN SDMMC2_Init 1 */

	/* USER CODE END SDMMC2_Init 1 */
	hsd2.Instance = SDMMC2;
	hsd2.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
	hsd2.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
	hsd2.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
	hsd2.Init.BusWide = SDMMC_BUS_WIDE_1B;
	hsd2.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd2.Init.ClockDiv = 0;
	/* USER CODE BEGIN SDMMC2_Init 2 */

	/* USER CODE END SDMMC2_Init 2 */

}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void) {

	/* USER CODE BEGIN TIM6_Init 0 */

	/* USER CODE END TIM6_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM6_Init 1 */

	/* USER CODE END TIM6_Init 1 */
	htim6.Instance = TIM6;
	htim6.Init.Prescaler = 9999;
	htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim6.Init.Period = 9999;
	htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM6_Init 2 */

	/* USER CODE END TIM6_Init 2 */

}

/**
 * @brief TIM7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM7_Init(void) {

	/* USER CODE BEGIN TIM7_Init 0 */

	/* USER CODE END TIM7_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM7_Init 1 */

	/* USER CODE END TIM7_Init 1 */
	htim7.Instance = TIM7;
	htim7.Init.Prescaler = 19999;
	htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim7.Init.Period = 9999;
	htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim7) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM7_Init 2 */

	/* USER CODE END TIM7_Init 2 */

}

/* FMC initialization function */
static void MX_FMC_Init(void) {

	/* USER CODE BEGIN FMC_Init 0 */

	/* USER CODE END FMC_Init 0 */

	FMC_SDRAM_TimingTypeDef SdramTiming = { 0 };

	/* USER CODE BEGIN FMC_Init 1 */

	/* USER CODE END FMC_Init 1 */

	/** Perform the SDRAM1 memory initialization sequence
	 */
	hsdram1.Instance = FMC_SDRAM_DEVICE;
	/* hsdram1.Init */
	hsdram1.Init.SDBank = FMC_SDRAM_BANK2;
	hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
	hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13;
	hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_32;
	hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
	hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_1;
	hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
	hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_DISABLE;
	hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
	hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
	/* SdramTiming */
	SdramTiming.LoadToActiveDelay = 16;
	SdramTiming.ExitSelfRefreshDelay = 16;
	SdramTiming.SelfRefreshTime = 16;
	SdramTiming.RowCycleDelay = 16;
	SdramTiming.WriteRecoveryTime = 16;
	SdramTiming.RPDelay = 16;
	SdramTiming.RCDDelay = 16;

	if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK) {
		Error_Handler();
	}

	/* USER CODE BEGIN FMC_Init 2 */

	/* USER CODE END FMC_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE()
	;
	__HAL_RCC_GPIOB_CLK_ENABLE()
	;
	__HAL_RCC_GPIOD_CLK_ENABLE()
	;
	__HAL_RCC_GPIOG_CLK_ENABLE()
	;
	__HAL_RCC_GPIOI_CLK_ENABLE()
	;
	__HAL_RCC_GPIOF_CLK_ENABLE()
	;
	__HAL_RCC_GPIOH_CLK_ENABLE()
	;
	__HAL_RCC_GPIOA_CLK_ENABLE()
	;
	__HAL_RCC_GPIOJ_CLK_ENABLE()
	;

	/*Configure GPIO pin : PI13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pin : PI15 */
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pin : PA0 */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

	//Vai buscar os valores de X e Y
	if (GPIO_Pin == GPIO_PIN_13 && touch_timer) {
		tsFlag = 1;
		touch_timer = 0;
		BSP_TS_GetState(&TS_State);
		touchXvalue = (int) TS_State.touchX[0];
		touchYvalue = (int) TS_State.touchY[0];
	}
	if (GPIO_Pin == GPIO_PIN_0) {
		blue_button = 1;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {

	//Está configurado para 1 segundo
	if (htim->Instance == TIM6) {
		play_timer--;
		total_time++;
		touch_timer = 1;
		timer_1s = 1;

	}

	if (htim->Instance == TIM7)
		timer7Flag = 1;

}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* adcHandle) {

	//ADC da temperatura
	if (adcHandle == &hadc1) {
		hadc1Value = HAL_ADC_GetValue(&hadc1);
	}
}

void LCD_Config_Start(void) {

	/* LCD DO MENU */
	uint32_t lcd_status = LCD_OK;

	/* Initialize the LCD */
	lcd_status = BSP_LCD_Init();
	while (lcd_status != LCD_OK)
		;

	BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);

	/* Clear the LCD */
	BSP_LCD_Clear(LCD_COLOR_WHITE);

	/* Set LCD Example description */
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 120);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font24);
	BSP_LCD_DisplayStringAt(0, 50, (uint8_t *) "REVERSI GAME", CENTER_MODE);

	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);

	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(35, 150, (uint8_t *) "SINGLEPLAYER", LEFT_MODE);
	BSP_LCD_DrawVLine(200, 0, 600);

	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(240, 150, (uint8_t *) "MULTIPLAYER", LEFT_MODE);
	BSP_LCD_DrawVLine(400, 0, 600);

	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(440, 150, (uint8_t *) "HIGH SCORES", LEFT_MODE);
	BSP_LCD_DrawVLine(600, 0, 600);

	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(670, 150, (uint8_t *) "RULES", LEFT_MODE);

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetFont(&Font24);
}

void LCD_Config_Rules(void) {

	char string[50];

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 120);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font24);
	BSP_LCD_DisplayStringAt(0, 50, (uint8_t *) "RULES", CENTER_MODE);

	BSP_LCD_SetFont(&Font16);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DisplayStringAt(0, 180,
			(uint8_t *) "Four pieces have to be placed in the central squares of the board, so",
			CENTER_MODE);
	BSP_LCD_DisplayStringAt(0, 200,
			(uint8_t *) "that each pair of pieces of the same color form a diagonal between them.",
			CENTER_MODE);
	BSP_LCD_DisplayStringAt(0, 220,
			(uint8_t *) "The player with red pieces moves first, only 1 move is made every turn.",
			CENTER_MODE);
	BSP_LCD_DisplayStringAt(0, 260,
			(uint8_t *) "If you don't know how to play, click everywhere till something happens.",
			CENTER_MODE);

	BSP_LCD_DisplayStringAt(0, 375,
			(uint8_t *) "Touch anywhere in the screen to get back to the MENU.",
			CENTER_MODE);

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillRect(0, 460, BSP_LCD_GetXSize(), 30);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	BSP_LCD_SetFont(&Font16);
	sprintf(string, "REVERSI GAME - DESIGNED BY (c) ALFONSO BRANDAO");
	BSP_LCD_DisplayStringAt(0, 460, (uint8_t *) string, CENTER_MODE);

}

void LCD_Gameplay(void) {

	draw_board();

	/* LEFT SIDE - PLAYER 1*/

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DrawHLine(20, 40, 160);
	BSP_LCD_DrawVLine(20, 40, 400);
	BSP_LCD_DrawHLine(20, 440, 160);
	BSP_LCD_DrawVLine(180, 40, 400);

	BSP_LCD_SetTextColor(LCD_COLOR_RED);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(30, 50, (uint8_t *) "PLAYER 1", LEFT_MODE);

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(30, 80, (uint8_t *) "SCORE: ", LEFT_MODE);

	/* RIGHT SIDE - PLAYER 2*/

	BSP_LCD_DrawHLine(620, 40, 160);
	BSP_LCD_DrawVLine(620, 40, 400);
	BSP_LCD_DrawHLine(620, 440, 160);
	BSP_LCD_DrawVLine(780, 40, 400);

	BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(630, 50, (uint8_t *) "PLAYER 2", LEFT_MODE);

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	BSP_LCD_SetFont(&Font16);
	BSP_LCD_DisplayStringAt(630, 80, (uint8_t *) "SCORE: ", LEFT_MODE);

}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
