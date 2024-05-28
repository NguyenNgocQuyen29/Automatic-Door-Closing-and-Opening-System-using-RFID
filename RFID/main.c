/*
* File: main.c
* Author: Nguyen Thi Ngoc Quyen
* Date: 20/01/2024
* Description: This project uses an RFID module to communicate with a microcontroller, which can be applied to many practical projects dêpnding on the user's needs
*/
#include "stm32f10x.h"                  // Device header
#include "stm32f1_rc522.h"
#include "liquidcrystal_i2c.h"
#include <stdio.h>

#define MAX_SLAVE_CARD                 50
#define FLASH_ADDRESS_SECTOR1          0x8008000U
#define FLASH_ADDRESS_SECTOR2          0x8008800U
#define FLASH_ADDRESS_SECTOR3          0x8009000U
#define SECTOR_SIZE                    2048
typedef enum
{
		DO_NOT_FIND_CARD = 0,
		SLAVE_CARD,
		MASTER_CARD
}CardInfoType;

typedef enum
{
		SECTOR1 	 = 0xFF01,
		SECTOR2 	 = 0xFF02,
		NO_SECTORS = 0xFFFF
}SectorActiveType;

typedef enum
{
    FALSE = 0,
    TRUE = 1
} Boolean;

typedef enum
{
	DO_NOTHING       = 0,
	ADD_SLAVE_CARD      ,
	REMOVE_SLAVE_CARD
}MasterCardIDStateType;


void I2C_LCD_Config(void);
void PWM_Init(void);
void Flash_Unlock(void);
void Flash_Erase(volatile uint32_t u32StartAddr);
void Flash_Write(volatile uint32_t u32StartAddr, uint8_t* u8BufferWrite, uint32_t u32Length);
void Flash_Read(volatile uint32_t u32StartAddr, uint8_t* u8BufferRead, uint32_t u32Length);
CardInfoType CheckCard(void);
void AddSlaveCard(void);
void RemoveSlaveCard(void);
void OpendDoor(void);
void ReadCardFromFlash(void);


static uchar Master_ID[5] = {0xC3, 0x91, 0x94, 0x25, 0xE3};
static uchar Slave_ID[50][5];
static uchar CardID[5];
static int8_t s8Time = 0;
static uint16_t checkActive = 0;
static SectorActiveType SectorActive = NO_SECTORS;
static MasterCardIDStateType CardIDForce = DO_NOTHING;
static CardInfoType CardIDInfo;
static MasterCardIDStateType CountStateMasterCard = DO_NOTHING;

int main(void)
{
	PWM_Init();
	/* Push Sevor to 0*/
	TIM4->CCR4 = 920; //timer 4 chnal 4: pulse width
	I2C_LCD_Config();
  HD44780_Init(2);
  HD44780_Clear();
	
	MFRC522_Init();	
	//Timer2RegisterInterupt();
	
	/* Check if do not have any active card in memory */
	Flash_Read(FLASH_ADDRESS_SECTOR3, (uint8_t *)&checkActive, 2U);
	if (checkActive == SECTOR2)
	{
		SectorActive = SECTOR2;		
	}
	else
	{
		SectorActive = SECTOR1;
	}
	
	ReadCardFromFlash();
	
	Delay_Ms(100);
	TIM4->CCR4 = 0;
	
	while(1) 
	{
		CardIDInfo = CheckCard();
		if ((CardIDInfo == MASTER_CARD) || (CardIDForce != DO_NOTHING))
		{
			CountStateMasterCard++;
			if ((CountStateMasterCard == ADD_SLAVE_CARD) || (CardIDForce == ADD_SLAVE_CARD))
			{
				AddSlaveCard();
			}
			else if ((CountStateMasterCard == REMOVE_SLAVE_CARD) || (CardIDForce == REMOVE_SLAVE_CARD))
			{
				RemoveSlaveCard();
			}
			else
			{
				CountStateMasterCard = DO_NOTHING;
				CardIDForce = DO_NOTHING;
				HD44780_Clear();
				HD44780_SetCursor(0,0);
				HD44780_PrintStr("Welcome To Room");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr(" Put Your Card");
				Delay_Ms(800);
			}
		}
		else if (CardIDInfo == SLAVE_CARD)
		{
			OpendDoor();
		}
		else
		{
			
		}
		
		if ((CountStateMasterCard == DO_NOTHING) && (CardIDForce == DO_NOTHING))
		{
			  HD44780_SetCursor(0,0);
				HD44780_PrintStr("Welcome To Room");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr(" Put Your Card");
		}
	}
}
/*
* Function: ReadCardFromFlash
* Description: This function is used to read information from Flash memory
*/
void ReadCardFromFlash(void){
  for (int i  = 0; i < MAX_SLAVE_CARD; i++)
	{
		if (SectorActive == SECTOR2)
		{
			Flash_Read((FLASH_ADDRESS_SECTOR2 + 8*i), &Slave_ID[i][0], 5U);
		}
		else
		{
			Flash_Read((FLASH_ADDRESS_SECTOR1 + 8*i), &Slave_ID[i][0], 5U);
		}
	}
}
/*
* Function: OpendDoor
* Description: This function is used to read information card if duplicate, it will control servo motor open door
*/
void OpendDoor(void)
{
	char Str[16];
	uint8_t u8Count = 0;
	Boolean u8CountExistCard = FALSE;
	
	MFRC522_Halt();
	if (MFRC522_ReadCardID(CardID) == MI_OK)
	{
	/* Scan all card */
		for (u8Count = 0; u8Count < 5; u8Count++)
		{
			if ((Slave_ID[u8Count][0] == CardID[0]) && (Slave_ID[u8Count][1] == CardID[1]) && (Slave_ID[u8Count][2] == CardID[2]) \
					 		&& (Slave_ID[u8Count][3] == CardID[3]) && (Slave_ID[u8Count][4] == CardID[4]) \
					 		)
			{
				u8CountExistCard = TRUE;
			}
		}
		
		if (u8CountExistCard == TRUE)
		{
			HD44780_Clear();
			HD44780_SetCursor(0,0);
			HD44780_PrintStr("Welcome To Room");
			HD44780_SetCursor(0,1);
			HD44780_PrintStr("Door Unlocked ");
			/* Opend Door */
			TIM4->CCR4 = 1650;
			Delay_Ms(2000);
			HD44780_Clear();
			s8Time = 9;
			/* Counter  = 0*/
			TIM2->CNT = 0U;
			while (s8Time > 0)
			{
				HD44780_SetCursor(0,0);
				HD44780_PrintStr("Door Will Close");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr("In ");
				sprintf(&Str[0], "%0.1d ", s8Time);
				HD44780_PrintStr(&Str[0]);
				HD44780_PrintStr("Sec HurryUp");
			}
			TIM4->CCR4 = 920;
			Delay_Ms(500);
			HD44780_Clear();
			TIM4->CCR4 = 0;
		}
		else
		{
			HD44780_Clear();
			HD44780_SetCursor(0,0);
			HD44780_PrintStr("Sorrry");
			HD44780_SetCursor(0,1);
			HD44780_PrintStr("Card Not Found! ");
			Delay_Ms(2000);
			HD44780_Clear();
		}
	}
}
/*
* Function: RemoveSlaveCard
* Description: This function is used for removing slave card information
* Input:
*		none
* Output:
*		card removed information
*/
void RemoveSlaveCard(void)
{
	uint8_t u8CountMaterID = 0;
	uint8_t u8Count = 0;
	uint8_t u8CountExistCard = 0;
	uint32_t u8Inter = 0;
	
	HD44780_Clear();
	Delay_Ms(200);
	
	while (1)
	{
		HD44780_SetCursor(0,0);
		HD44780_PrintStr("Del Slave Card");
		HD44780_SetCursor(0,1);
		HD44780_PrintStr("Waiting ...");
		/* Clear */
		u8CountMaterID = 0;
		u8CountExistCard = 0;
		
		if (MFRC522_ReadCardID(CardID) == MI_OK)
		{
			for (u8Count = 0; u8Count < 5; u8Count++)
			{
				if(Master_ID[u8Count] == CardID[u8Count])
				{
					u8CountMaterID++;
				}
			}
			if (u8CountMaterID != 5)
			{
				 /* Scan all card if it not already exist */
				for (u8Count = 0; u8Count < 5; u8Count++)
				{
					 /* Scan all card if it already exist */
					 if ((Slave_ID[u8Count][0] == CardID[0]) && (Slave_ID[u8Count][1] == CardID[1]) && (Slave_ID[u8Count][2] == CardID[2]) \
					 		&& (Slave_ID[u8Count][3] == CardID[3]) && (Slave_ID[u8Count][4] == CardID[4]) \
					 		)
					 	{
							/* Remove Card */
							Slave_ID[u8Count][0] = 0xFF;
							Slave_ID[u8Count][1] = 0xFF;
							Slave_ID[u8Count][2] = 0xFF;
							Slave_ID[u8Count][3] = 0xFF;
							Slave_ID[u8Count][4] = 0xFF;
							
							/* Remove from flash */
							
								if (SectorActive == SECTOR2)
								{
										SectorActive = SECTOR1;
										Flash_Erase(FLASH_ADDRESS_SECTOR1);
										for (u8Inter = 0; u8Inter < 5; u8Inter++)
										{
											Flash_Write((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&Slave_ID[u8Inter], 6U);
										}
										Flash_Erase(FLASH_ADDRESS_SECTOR3);
										Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
										Flash_Erase(FLASH_ADDRESS_SECTOR2);
								}
								else
								{
									SectorActive = SECTOR2;
									Flash_Erase(FLASH_ADDRESS_SECTOR2);
									for (u8Inter = 0; u8Inter < 5; u8Inter++)
								  {
										Flash_Write((FLASH_ADDRESS_SECTOR2 + u8Inter*8), (uint8_t *)&Slave_ID[u8Inter], 6U);
									}
									Flash_Erase(FLASH_ADDRESS_SECTOR3);
								  Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
									Flash_Erase(FLASH_ADDRESS_SECTOR1);
								}
							
					 		HD44780_Clear();
					 		HD44780_SetCursor(0,0);
					 		HD44780_PrintStr("Del Slave Card ");
					 		HD44780_SetCursor(0,1);
					 		HD44780_PrintStr("Success!!!");
					 		Delay_Ms(1000);
					 		HD44780_Clear();
							u8CountExistCard++;
							if (u8CountExistCard > 0)
							{
								break;
							}
				   	}
				}
				/* Card not exist and need to add slave card */
				if (u8CountExistCard == 0)
				{
					HD44780_Clear();
					HD44780_SetCursor(0,0);
					HD44780_PrintStr("Add Slave Card ");
					HD44780_SetCursor(0,1);
					HD44780_PrintStr("Not Added Yet");
					Delay_Ms(1000);
					HD44780_Clear();
				}
			}
			else
			{
				CardIDForce = DO_NOTHING;
				break;
			}
		}
		
		if (CardIDForce == DO_NOTHING)
		{
		  Delay_Ms(1000);
			break;
		}
	}
}
/*
* Function: AddSlaveCard
* Description: This function is used for adding slave card information
* Input:
*		none
* Output:
*		card added information
*/
void AddSlaveCard(void){
	uint8_t countMasterID = 0;
	uint8_t countSlaveCard = 0;
	uint8_t countExistCard = 0;
	uint8_t ReadCardID[5] = {0};
	
	
	HD44780_Clear();
	Delay_Ms(200);
	
	while(1){
		HD44780_SetCursor(0,0);
		HD44780_PrintStr("Add Slave Card");
		HD44780_SetCursor(0,1);
		HD44780_PrintStr("Wating ...");
		
		countMasterID = 0;
		countExistCard = 0;
		
		if(MFRC522_ReadCardID(CardID) == MI_OK){
				for(int i = 0; i < 5; i++){
						if(Master_ID[i] == CardID[i]){
								countMasterID++;
						}
				}
				if(countMasterID != 5){
						/* Scan all card */
						for(int i = 0; i < 5;i++){
								if((Slave_ID[i][0] == CardID[0]) && (Slave_ID[i][1] == CardID[1]) && (Slave_ID[i][2] == CardID[2]) && (Slave_ID[i][3] == CardID[3]) && (Slave_ID[i][4] == CardID[4])){
												HD44780_Clear();
												HD44780_SetCursor(0,0);
												HD44780_PrintStr("Add Slave Card");
												HD44780_SetCursor(0,1);
												HD44780_PrintStr("Card Was Added");
												Delay_Ms(1000);
												HD44780_Clear();
												countExistCard++;
												if(countExistCard > 0){
														break;
												}
										}
								}
				/* Card not exist*/
				if(countExistCard == 0){
						for(int i = 0; i < 5; i++){
								if((Slave_ID[i][0] == 0xFF) && (Slave_ID[i][1] == 0xFF)&& (Slave_ID[i][2] == 0xFF) && (Slave_ID[i][3] == 0xFF) && (Slave_ID[i][4] == 0xFF)){
											for(int j=0;j<5;j++){
													Slave_ID[i][j] = CardID[j];
											}
											for(int j = 0;j < SECTOR_SIZE;j++){
													if(SectorActive == SECTOR2){
															Flash_Read((FLASH_ADDRESS_SECTOR2 + j*8), &ReadCardID[0], 5U);
															if ((ReadCardID[0] == 0xFF) && (ReadCardID[1] == 0xFF) && (ReadCardID[2] == 0xFF) && (ReadCardID[3] == 0xFF) && (ReadCardID[4] == 0xFF))
															{
																Flash_Write((FLASH_ADDRESS_SECTOR2 + j*8), (uint8_t *)&CardID[0], 8U);
																Flash_Erase(FLASH_ADDRESS_SECTOR3);
																Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
																break;
															}
													}else{
															Flash_Read((FLASH_ADDRESS_SECTOR1 + j*8), (uint8_t *)&ReadCardID[0], 5U);
															if ((ReadCardID[0] == 0xFF) && (ReadCardID[1] == 0xFF) && (ReadCardID[2] == 0xFF) && (ReadCardID[3] == 0xFF) && (ReadCardID[4] == 0xFF))
															{
																Flash_Write((FLASH_ADDRESS_SECTOR1 + j*8), (uint8_t *)&CardID[0], 8U);
																Flash_Erase(FLASH_ADDRESS_SECTOR3);
																Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
																break;
															}
													}
											}
											HD44780_Clear();
											HD44780_SetCursor(0,0);
											HD44780_PrintStr("Add Slave Card ");
											HD44780_SetCursor(0,1);
											HD44780_PrintStr("Success!!!");
											Delay_Ms(1000);
											HD44780_Clear();
											break;
								}
								else{
										countSlaveCard++;
								}
						}
						if(countSlaveCard == 5){
								//card exist
								HD44780_Clear();
								HD44780_SetCursor(0,0);
								HD44780_PrintStr("Add Slave Card ");
								HD44780_SetCursor(0,1);
								HD44780_PrintStr("Failure!!!!!");
								Delay_Ms(1000);
								HD44780_Clear();
						}
				}
					
			}
				else{
						CardIDForce = REMOVE_SLAVE_CARD;
						break;
				}
		}
		if(CardIDForce == REMOVE_SLAVE_CARD){
					break;
		}
	}
}
/*
* Function: CheckCard
* Description: this function checks card whether master or slave
* Input:
*		none
* Output:
*		infor - a CardInfoType datatype - type card is master or slave
*/
CardInfoType CheckCard(void){
		CardInfoType info = DO_NOT_FIND_CARD;
		uint8_t countMaster = 0;
		if(MFRC522_ReadCardID(CardID) == MI_OK){
			/* Master Card*/
			for(int i=0; i < 5;i++){
					if(Master_ID[i] == CardID[i]){
							countMaster++;
					}
			}
			if(countMaster == 5) //master
			{
				info = MASTER_CARD;
			}else{
				info = SLAVE_CARD;
			}
		}
		return info;
}
	
/*
* Function: I2C_LCD_Config
* Description: This function config I2C to display infor in LCD
* Input: 
*		none
* Output:
*		none
*/
void I2C_LCD_Config(void){
		GPIO_InitTypeDef GPIO_Config;
		I2C_InitTypeDef I2C_Config;
	
		/* PortB clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		GPIO_StructInit(&GPIO_Config);
	  /* I2C1 clock enable */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
		I2C_StructInit(&I2C_Config);
		
		/* Config GPIO: SCL(PB6) , SDA(PB7)*/
		GPIO_Config.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
		GPIO_Config.GPIO_Mode = GPIO_Mode_AF_OD;
		GPIO_Config.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOB, &GPIO_Config);
		/* Config I2C */
	  I2C_Config.I2C_Mode = I2C_Mode_I2C;
		I2C_Config.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_Config.I2C_OwnAddress1 = 0x00;
    I2C_Config.I2C_Ack = I2C_Ack_Enable;
    I2C_Config.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Config.I2C_ClockSpeed = 100000;
		
		I2C_Init(I2C_Chanel, &I2C_Config);
    I2C_Cmd(I2C_Chanel, ENABLE);
}
/*
* Function: PWM_Init
* Description: This function uses TIM4 to genare pulse to control Servo motor
* Input: 
*		none
* Output:
*		none
*/
void PWM_Init(void){
		/* Initialization struct */
		TIM_TimeBaseInitTypeDef TIM_config;
		TIM_OCInitTypeDef TIM_OCConfig;
		GPIO_InitTypeDef GPIO_Config;
		/* Initialize TIM4 */
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
		/* Create PWM 50HZ*/
	  /* Prescale timer clock from 72Mhz(APB1) to 720kHz by prescaler 100*/
		TIM_config.TIM_Prescaler = 100;
		/* Period = (timer_clock/f_PWM)*/
		/* Period = 720kHz / 50 - 1 = 14399 */
		TIM_config.TIM_Period = 14399;
		TIM_config.TIM_ClockDivision = TIM_CKD_DIV1;
		TIM_config.TIM_CounterMode = TIM_CounterMode_Up;
		TIM_TimeBaseInit(TIM4, &TIM_config);
	
		/* Enable TIM4 */
		TIM_Cmd(TIM4, ENABLE);
		
		/* Config PWM1 */
		/* PWM1 genered from TIM4 */
		TIM_OCConfig.TIM_OCMode = TIM_OCMode_PWM1; 
		TIM_OCConfig.TIM_OCNIdleState = TIM_OutputState_Enable;
		TIM_OCConfig.TIM_OCPolarity = TIM_OCPolarity_High;
		TIM_OCConfig.TIM_Pulse = 0;
		TIM_OC4Init(TIM4, &TIM_OCConfig);
		TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
		/* Config GPIO*/
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		GPIO_Config.GPIO_Pin = GPIO_Pin_9;
		GPIO_Config.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_Config.GPIO_Speed = GPIO_Speed_2MHz;
		GPIO_Init(GPIOB, &GPIO_Config);
}
/***************************************************************************************
*                                     FLASH                                            *
***************************************************************************************/
/*
* Function: Flash_Unlock
* Description: This sequence consists of two write cycles, where two key values (KEY1 and KEY2) are written to the FLASH_KEYR address - page 12/31 RM flash stmf1
*/
void Flash_Unlock(void)
{
    /* This sequence consists of two write cycles, where two key values (KEY1 and KEY2) are written to the FLASH_KEYR address*/
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;
}
/*
* Function: Flash_Erase
* Description: This function is used to erase each page in flash memory stm32f1 - reference page 15/31 rm flash memory
* Input:
*		u32StartAddr - uint32_t datatype - address of page want to erase
*	Output:
* 	none return
*/
void Flash_Erase(volatile uint32_t u32StartAddr)
{
    /* Check that no Flash memory operation is ongoing by checking the BSY bit in the FLASH_CR register */
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);

    /* Check unlock sequences */
    if ((FLASH->CR&FLASH_CR_LOCK) == FLASH_CR_LOCK )
    {
        Flash_Unlock();
    }

    /* Set the PER bit in the FLASH_CR register */
    FLASH->CR |= FLASH_CR_PER;
    /* Program the FLASH_AR register to select a page to erase */
    FLASH->AR = u32StartAddr;
    /* Set the STRT bit in the FLASH_CR register */
    FLASH->CR |= FLASH_CR_STRT;
    /* Wait for the BSY bit to be reset */
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    /* Clear PER bit in the FLASH_CR register */
    FLASH->CR &= ~(uint32_t)FLASH_CR_PER;
    /* Clear STRT bit in the FLASH_CR register */
    FLASH->CR &= ~(uint32_t)FLASH_CR_STRT;

}
/*
* Function: Flash_Write
* Description: This function is used to write into flash memory
* Input:
*		u32StartAddr - uint32_t datatype - start address of where want to write
*		u8BufferWrite - 
*		u32Length - uint32_t datatype - lengh of buffer want to write into flash
*/
void Flash_Write(volatile uint32_t u32StartAddr, uint8_t* u8BufferWrite, uint32_t u32Length)
{
    uint32_t u32Count = 0U;

    /* Check input paras */
    if((u8BufferWrite == 0x00U) || (u32Length == 0U) || u32Length%2U != 0U)
    {
        return;
    }
    /* Check that no Flash memory operation is ongoing by checking the BSY bit in the FLASH_CR register */
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY)
    {
        /*  Wating for Bsy bit */
    }
    /* Check unlock sequences */
    if ((FLASH->CR&FLASH_CR_LOCK) == FLASH_CR_LOCK )
    {
        Flash_Unlock();
    }

    /* Write FLASH_CR_PG to 1 */
    FLASH->CR |= FLASH_CR_PG;

    /* Perform half-word write at the desired address*/
    for(u32Count = 0U; u32Count < (u32Length/2); u32Count ++ )
    {
        *(uint16_t*)(u32StartAddr + u32Count*2U) = *(uint16_t*)((uint32_t)u8BufferWrite + u32Count*2U);
        /* Wait for the BSY bit to be reset */
        while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    }

    /* Clear PG bit in the FLASH_CR register */
    FLASH->CR &= ~(uint32_t)FLASH_CR_PG;

}
void Flash_Read(volatile uint32_t u32StartAddr, uint8_t* u8BufferRead, uint32_t u32Length)
{

    /* Check input paras */
    if((u8BufferRead == 0x00U) || (u32Length == 0U))
    {
        return;
    }
    do
    {
        if(( u32StartAddr%4U == 0U) && ((uint32_t)u8BufferRead%4U == 0U) && (u32Length >= 4U))
        {
            *(uint32_t*)((uint32_t)u8BufferRead) = *(uint32_t*)(u32StartAddr);
            u8BufferRead = u8BufferRead + 4U;
            u32StartAddr = u32StartAddr + 4U;
            u32Length = u32Length - 4U;
        }
        else if(( u32StartAddr%2U == 0U) && ((uint32_t)u8BufferRead%2U == 0U) && (u32Length >= 2U))
        {
            *(uint16_t*)((uint32_t)u8BufferRead) = *(uint16_t*)(u32StartAddr);
            u8BufferRead = u8BufferRead + 2U;
            u32StartAddr = u32StartAddr + 2U;
            u32Length = u32Length - 2U;
        }
        else
        {
            *(uint8_t*)(u8BufferRead) = *(uint8_t*)(u32StartAddr);
            u8BufferRead = u8BufferRead + 1U;
            u32StartAddr = u32StartAddr + 1U;
            u32Length = u32Length - 1U;
        }
    }
    while(u32Length > 0U);
}	

