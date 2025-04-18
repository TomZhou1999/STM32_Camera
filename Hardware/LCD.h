#ifndef __LCD_H
#define __LCD_H

/*
BLK - PA2	//背光控制
CS - PA0 	//0激活设备
DC - PA3	//控制0 或者 数据1
RES - PA1 	//reset
SDA - PA7 
SCL - PA5 
VCC - 3.3V
GND - GND
*/

/*上电*/
void LCD_W_BLK(uint8_t BitValue);
void LCD_W_DC(uint8_t BitValue);
void LCD_W_RES(uint8_t BitValue);
void LCD_W_CS(uint8_t BitValue);
void LCD_GPIO_Init(void);

/*初始化*/
void LCD_SPI_W_SS(uint8_t BitValue);
void LCD_SPI_Init(void);
void LCD_SPI_Start(void);
void LCD_SPI_Stop(void);
uint8_t LCD_SPI_SwapBytpe(uint8_t ByteSend);
void LCD_WriteCommand(uint8_t Command);
void LCD_WriteData(uint8_t Data);
void LCD_Init(void);

/*显示功能*/
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_SetPos(uint16_t x, uint16_t y);
void LCD_Clear(uint16_t Color);
void LCD_Image(uint16_t Color);
#endif
