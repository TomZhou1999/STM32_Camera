#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "SPI_Camera.h"
#include "sensor_BF30A2.h"
#include "LCD.h"

uint8_t SID;
uint8_t Y_aver;


int main(void)
{
	//激活设备
	/* OLED*/
	OLED_Init();
	OLED_Clear();	
	OLED_ShowString(0, 0, "ID:", OLED_8X16);
	OLED_ShowString(0, 17, "Y:", OLED_8X16);
	
	/*sensor*/
	Sensor_BF30A2Init();
	SID = BF30A2_GetID();
	OLED_ShowHexNum(30, 0, SID, 2, OLED_8X16);	
	OLED_Update();
	/* LCD */
	LCD_Init();
	LCD_SetWindow(0,0,239,239);
	LCD_Clear(0x0000);	
	ImageData_Recog();							//识别图像数据
	ImageData_Transfer();						//转运和识别数据
	while (1)
	{
		//Y_aver = BF30A2_ReadReg(0x88);
		//OLED_ShowHexNum(30, 17, Y_aver, 2, OLED_8X16);
		//OLED_Update();
	}
}
