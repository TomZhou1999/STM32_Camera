#ifndef __SPI_CAMERA_H
#define __SPI_CAMERA_H

#define Sensor_Pin_MCLK GPIO_Pin_8		//PA8 MCO/PWM
#define Sensor_Pin_PWD GPIO_Pin_1		  //PB1 
#define Sensor_Pin_SDA GPIO_Pin_11		//PB11 HARDWARE I2C
#define Sensor_Pin_SCL GPIO_Pin_10		//PB10 HARDWARE I2C
#define Sensor_Pin_PCLK GPIO_Pin_5		//PA8 HARDWARE SPI
#define Sensor_Pin_Data0 GPIO_Pin_6		//PA8 HARDWARE SPI

#include <stdint.h>
/** 
	* 接口定义：适配淘宝上的OV7670模组
  * 
  *                           
  *             .-----.
  *       2.8V -|。   |- GND
  *(PA10) SCL  -|     |- SDA	(PA11)
  *            -|     |- 
  * (PB13)SLCK -|     |- XCLK	(PA8)
  *            -|     |-
  *            -|     |- D1   (PB15)
  *            -|     |- D0		(PB14)
  *       	   -|     |- PDN	(PB1)
  *             .-----.
  * 
  */


typedef struct
{
    uint8_t SENSOR_ADDRESS;      /**< SENSOR I2C设备地址 */
    uint8_t SENSOR_ID_ADDRESS;   /**< SENSOR ID寄存器地址 */
    uint8_t SENSOR_ID;          /**< SENSOR ID */
		uint16_t Image_Width;		  	/**< 宽度靠识别 */
		uint16_t Image_Height;			/**< 高度可调整 */
} SENSOR_InitTypeDef;


/*Power Sequence*/
void XCLKSetFrquence(uint8_t frq);
void XCLKInit(void);
void PWDInit(void);
void PWDSet(uint8_t state);
void SensorPowerON(void);

/*Init*/
void SENSOR_WaitEvent(I2C_TypeDef* I2Cx, uint32_t I2C_EVENT);
void SENSOR_WriteReg(uint8_t RegAddress, uint8_t Data);
uint8_t SENSOR_ReadReg(uint8_t RegAddress);
void SENSOR_I2C_Init(void);



/*Data Transform*/
void Cmos_SPI_W_SS(uint8_t BitValue);
void CmoSPi_Init(void);
uint8_t ReadBuffer(uint8_t i);
uint8_t Cmos_SPI_Rx(void);
void ImageData_Transfer(void);

void MyDMA_Init(uint32_t AddrA, uint32_t AddrB, uint16_t Size);
void DMA_Switch_Buffer(uint32_t Buffer);


uint8_t *Buffer_Swap(void);
uint16_t ImageData_Size(void);

void ImageData_ClearFlag(void);
void ImageData_Recog(void);
void ImageData_Transfer(void);
uint8_t ImageData_ReadBuffer(void);
void Show_Image(void);

/*Drive*/
void SENSOR_Init(SENSOR_InitTypeDef * InitStruture);
uint8_t SENSOR_GetID(void);

#endif
