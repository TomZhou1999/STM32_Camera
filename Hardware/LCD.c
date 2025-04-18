#include "stm32f10x.h"                  // Device header
#include "Delay.h"


uint8_t frame_Flag = 0;

void LCD_W_BLK(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_2,(BitAction)BitValue);
}

void LCD_W_DC(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_3,(BitAction)BitValue);
}

void LCD_W_RES(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_1,(BitAction)BitValue);
}

void LCD_W_CS(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_0,(BitAction)BitValue);
}

/**
	* @brief 	LCD引脚初始化
  * @ref 		NULL
  * @return	NULL           
  */

void LCD_GPIO_Init()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_0 ;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	LCD_W_RES(0);
	LCD_W_DC(0);
	LCD_W_BLK(0);
	LCD_W_CS(0);
}

void LCD_SPI_W_SS(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_4,(BitAction)BitValue);
}

/**
  * @brief 	SPI1初始化,[STM32主机][LCD从机][全双工][mode0]
  * @ref 		NULL
  * @return	NULL
  */
void LCD_SPI_Init()
{	
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1,ENABLE);			//开启SPI1时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//推挽输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;			//上拉
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//复用推挽，交给片上外设
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);		
	
	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;												//主机
	SPI_InitStructure.SPI_Direction=SPI_Direction_1Line_Tx;							//stm32 TX
	SPI_InitStructure.SPI_DataSize=SPI_DataSize_8b;											//8bit
	SPI_InitStructure.SPI_FirstBit=SPI_FirstBit_MSB;										//高位在前
	SPI_InitStructure.SPI_BaudRatePrescaler=SPI_BaudRatePrescaler_2;		//72M/2=36MHz
	SPI_InitStructure.SPI_CPOL=SPI_CPOL_High;														//空闲高电平
	SPI_InitStructure.SPI_CPHA=SPI_CPHA_1Edge;													//第一边缘采样(移入)
	SPI_InitStructure.SPI_NSS=SPI_NSS_Soft;															//多主机控制
	SPI_InitStructure.SPI_CRCPolynomial=7;															//校验？
	SPI_Init(SPI1,&SPI_InitStructure);
	SPI_Cmd(SPI1,ENABLE);
	
	LCD_SPI_W_SS(1);
}




/**
* 函    数：读数据缓冲器
  * 参    数：NULL
  * 返 回 值：NULL
  */

void LCD_SPI_Start(void)
{
	LCD_SPI_W_SS(0);
}


void LCD_SPI_Stop(void)
{
	LCD_SPI_W_SS(1);
}

uint8_t LCD_SPI_SwapBytpe(uint8_t ByteSend)
{	
	while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_TXE)!=SET);
	SPI_I2S_SendData(SPI1,ByteSend);
	//while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_RXNE)!=SET);
	return 0;
}

/**
  * 函    数：LCD写命令
  * 参    数：Command 要写入的命令值，范围：0x00~0xFF
  * 返 回 值：NULL
  */
void LCD_WriteCommand(uint8_t Command)
{
	LCD_SPI_Start();							//起始
	LCD_W_DC(0);									//发送OLED的I2C从机地址
	LCD_SPI_SwapBytpe(Command);		//写入指定的命令
	LCD_SPI_Stop();								//终止
}

/**
  * 函    数：LCD写数据
  * 参    数：Data 要写入的数据值，范围：0x00~0xFF
  * 返 回 值：NULL
  */
void LCD_WriteData(uint8_t Data)
{
	LCD_SPI_Start();						//起始
	LCD_W_DC(1);								//发送OLED的I2C从机地址
	LCD_SPI_SwapBytpe(Data);		//写入指定的命令
	LCD_SPI_Stop();							//终止
}

/**
  * 函    数：LCD初始化
  * 参    数：NULL
  * 返 回 值：NULL
	* 说    明：使用前，需要调用此初始化函数,上电流程
  */
void LCD_Init()
{
	LCD_GPIO_Init();
	LCD_SPI_Init();
	Delay_ms(200);
	LCD_W_RES(1);
	LCD_W_BLK(1);
	LCD_WriteCommand(0x11); 	
	Delay_ms(120);
	
    LCD_WriteCommand(0x3A);     
    LCD_WriteData(0x55);  // 16bit/pixel RGB565格式
    LCD_WriteCommand(0xC5); 	
    LCD_WriteData(0x1A);
    LCD_WriteCommand(0x36);     
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xb2);	
    LCD_WriteData(0x05);
    LCD_WriteData(0x05);
    LCD_WriteData(0x00);
    LCD_WriteData(0x33);
    LCD_WriteData(0x33);

    LCD_WriteCommand(0xB7);			
    LCD_WriteData(0x05);			
    LCD_WriteCommand(0xBB);
    LCD_WriteData(0x3F);

    LCD_WriteCommand(0xC0); 
    LCD_WriteData(0x2c);

    LCD_WriteCommand(0xC2);	
    LCD_WriteData(0x01);

    LCD_WriteCommand(0xC3);	
    LCD_WriteData(0x0F);	

    LCD_WriteCommand(0xC4);	
    LCD_WriteData(0x20);	

    LCD_WriteCommand(0xC6);	
    LCD_WriteData(0X01);	

    LCD_WriteCommand(0xd0);	
    LCD_WriteData(0xa4);
    LCD_WriteData(0xa1);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x03);

    LCD_WriteCommand(0xE9);	
    LCD_WriteData(0x09);
    LCD_WriteData(0x09);
    LCD_WriteData(0x08);
    LCD_WriteCommand(0xE0); 
    LCD_WriteData(0xD0);
    LCD_WriteData(0x05);
    LCD_WriteData(0x09);
    LCD_WriteData(0x09);
    LCD_WriteData(0x08);
    LCD_WriteData(0x14);
    LCD_WriteData(0x28);
    LCD_WriteData(0x33);
    LCD_WriteData(0x3F);
    LCD_WriteData(0x07);
    LCD_WriteData(0x13);
    LCD_WriteData(0x14);
    LCD_WriteData(0x28);
    LCD_WriteData(0x30);
	
    LCD_WriteCommand(0XE1);
    LCD_WriteData(0xD0);
    LCD_WriteData(0x05);
    LCD_WriteData(0x09);
    LCD_WriteData(0x09);
    LCD_WriteData(0x08);
    LCD_WriteData(0x03);
    LCD_WriteData(0x24);
    LCD_WriteData(0x32);
    LCD_WriteData(0x32);
    LCD_WriteData(0x3B);
    LCD_WriteData(0x14);
    LCD_WriteData(0x13);
    LCD_WriteData(0x28);
    LCD_WriteData(0x2F);

    LCD_WriteCommand(0x21); 		
   
    LCD_WriteCommand(0x29);  
	
}

/**
  * 函    数：设置显示窗口范围
  * 参    数：(X1,X2)|(y1,y2)
  * 返 回 值：NULL
	* 说    明：使用前，需要调用此初始化函数,上电流程
  */
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_WriteCommand(0x2A);
    LCD_WriteData(x1 >> 8);
    LCD_WriteData(x1);
    LCD_WriteData(x2 >> 8);
    LCD_WriteData(x2);

    LCD_WriteCommand(0x2B);
    LCD_WriteData(y1 >> 8);
    LCD_WriteData(y1);
    LCD_WriteData(y2 >> 8);
    LCD_WriteData(y2);

    LCD_WriteCommand(0x2C);	//连续模式
}

void LCD_SetPos(uint16_t x, uint16_t y)
{
	LCD_SetWindow(x, y, x, y);
}
/*********************************************


**********************************************/

void LCD_Clear(uint16_t Color)
{
	uint32_t total_pixels = 57600;
	uint16_t color16 = Color;
	LCD_W_DC(1);
	LCD_SPI_Start();	
	//连续写入像素数据
	for(uint32_t i=0; i<total_pixels; i++) {
		LCD_SPI_SwapBytpe(color16 >> 8);  // 高字节
		LCD_SPI_SwapBytpe(color16 & 0xFF);// 低字节
	}
	LCD_SPI_Stop();
}

void LCD_Image(uint16_t Color)
{	
	uint32_t total_pixels = 128 * 48;
	uint16_t color16 = (Color>>3)<<11|(Color >> 2) << 5|Color >> 3;
	LCD_W_DC(1);
	LCD_SPI_Start();	
	//连续写入像素数据
	for(uint32_t i=0; i<total_pixels; i++) {
		LCD_SPI_SwapBytpe(color16 >> 8);  // 高字节
		LCD_SPI_SwapBytpe(color16 & 0xFF);// 低字节
	}
	LCD_SPI_Stop();
}

