#include "stm32f10x.h"                  // Device header
#include "SPI_Camera.h"
#include "LCD.h"
#include "OLED.h"

#define PWM 1	  					//置1使用PWM输出时钟 置0用MCO

uint8_t SENSOR_ADDRESS =0xdc;		//SENSOR的I2C从机地址
uint8_t SENSOR_ID_ADDRESS =0xfc;
uint8_t SENSOR_ID = 0x3B;
uint16_t Image_Width = 128;		  	//图像分辨率 宽度识别
uint16_t Image_Height = 48;			//高度可调整
uint16_t DataSize;					//图像数据大小、DMA转运大小


uint8_t e_buffer[8500] = {0x00};				//存储偶数行 	//分辨率降到(128+12)*48=6720
uint8_t o_buffer[8500] = {0x00};				//存储奇数行
uint32_t total_pixels = 6144;					//像素总量 		128*48=6144
uint32_t counter = 12;							//MTK数据量的间隔
uint16_t color16;
uint8_t *pixel_buffer = e_buffer;	
uint8_t sync_code = 0x00;						//数据识别码
uint32_t V_current = 0x00;						//帧数量
uint8_t V_DataID = 0x00;						//数据包格式

///////////////*Power Sequence*//////////////

/**
  *@brief 	修改XCLK时钟频率,单位MHZ 
  *@ref 		输入值-1为36的因子
  *@return 	NULL
  */
void XCLKSetFrquence(uint8_t frq)
{
	TIM_PrescalerConfig(TIM1, (int)(36/frq-1), TIM_PSCReloadMode_Immediate);
}



/**
  *@brief 	XCLK时钟初始化,默认时钟是6Mhz [PWM][TIM1]
  *@ref 	NULL
  *@return 	NULL
  */
void XCLKInit(void)
{	
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);			//开启TIM1的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);			//开启GPIOA的时钟
		
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;						//受外设控制的引脚，均需要配置为复用模式
	GPIO_InitStructure.GPIO_Pin = Sensor_Pin_MCLK;		
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;					//驱动能力
	GPIO_Init(GPIOA, &GPIO_InitStructure);										//将PA0引脚初始化为复用推挽输出	
																																
	if (PWM==1){
	/*配置时钟源*/
	TIM_InternalClockConfig(TIM1);														//选择TIM2为内部时钟，若不调用此函数，TIM默认也为内部时钟
	
	/*时基单元初始化*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;				//定义结构体变量
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //时钟分频，选择不分频，此参数用于配置滤波器时钟，不影响时基单元功能
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; //计数器模式，选择向上计数
	TIM_TimeBaseInitStructure.TIM_Period = 3 - 1;							//计数周期，即ARR的值
	TIM_TimeBaseInitStructure.TIM_Prescaler = 4 - 1;					//预分频器，即PSC的值 72M/2/6=6M
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;      //重复计数器，高级定时器才会用到
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);       //将结构体变量交给TIM_TimeBaseInit，配置TIM2的时基单元
	
	/*输出比较初始化*/
	TIM_OCInitTypeDef TIM_OCInitStructure;										//定义结构体变量
	TIM_OCStructInit(&TIM_OCInitStructure);										//结构体初始化，若结构体没有完整赋值, 则最好执行此函数，给结构体所有成员都赋一个默认值,避免结构体初值不确定的问题																																																											
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;					//输出比较模式，选择PWM模式1
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	//输出极性，选择为高，若选择极性为低，则输出高低电平取反
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;	//输出使能
	TIM_OCInitStructure.TIM_Pulse = 0;												//初始的CCR值
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);									//将结构体变量交给TIM_OC1Init，配置TIM1的输出比较通道1
	
	/*TIM使能*/
	TIM_Cmd(TIM1, ENABLE);																		//使能TIM2，定时器开始运行
	TIM_SetCompare1(TIM1, 1);																	//设置CCR1的值
	TIM_CtrlPWMOutputs(TIM1, ENABLE); 												// 启用高级定时器 PWM 输出
	}
	else
	{
		RCC_MCOConfig(RCC_MCO_HSI);															//只能输出8M
	}
	
}


/**
  *@brief 	PWDN初始化
  *@ref 	
  *@return 	NULL
  */
void PWDInit(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);				//开启GPIOB的时钟
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = Sensor_Pin_PWD;		
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;					//驱动能力
	GPIO_Init(GPIOB, &GPIO_InitStructure);								//将PA0引脚初始化为复用推挽输出					
	GPIO_ResetBits(GPIOB, Sensor_Pin_PWD);								//设置PWDN引脚为高电平
}


/**
  *@brief 	PWDN控制
  *@ref 	Sensor_Pin_PWD(PB1)
  *@return 	NULL
  */
void PWDSet(uint8_t state)
{
	GPIO_WriteBit(GPIOB,Sensor_Pin_PWD,(BitAction)state);
}


/**
  *@brief 	上电时序 电源常供
  *@ref 	NULL
  *@return 	NULL
  */
void SensorPowerON(void)
{
	XCLKInit();
	PWDInit();
	PWDSet(0);
}




///////////////*Init_I2C*//////////////

/**
  * @brief ：SENSOR等待事件
  * @ref 	：同I2C_CheckEvent
  * @return：NULL
  */
void SENSOR_WaitEvent(I2C_TypeDef* I2Cx, uint32_t I2C_EVENT)
{
	uint32_t Timeout;
	Timeout = 10000;									//给定超时计数时间
	while (I2C_CheckEvent(I2Cx, I2C_EVENT) != SUCCESS)	//循环等待指定事件
	{
		Timeout --;										//等待时，计数值自减
		if (Timeout == 0)								//自减到0后，等待超时
		{
			/*超时的错误处理代码，可以添加到此处*/
			break;										//跳出等待，不等了
		}
	}
}

/**
  * @brief 	SENSOR写寄存器
  * @ref 	RegAddress 寄存器地址，范围：参考SENSOR手册的寄存器描述 要写入寄存器的数据，范围：0x00~0xFF
  * @return NULL
  */

void SENSOR_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	I2C_GenerateSTART(I2C2, ENABLE);										//硬件I2C生成起始条件
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT);					//等待EV5
	
	I2C_Send7bitAddress(I2C2, SENSOR_ADDRESS, I2C_Direction_Transmitter);	//硬件I2C发送从机地址，方向为发送
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);	//等待EV6
	
	I2C_SendData(I2C2, RegAddress);											//硬件I2C发送寄存器地址
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTING);			//等待EV8
	
	I2C_SendData(I2C2, Data);												//硬件I2C发送数据
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED);				//等待EV8_2
	
	I2C_GenerateSTOP(I2C2, ENABLE);											//硬件I2C生成终止条件
}


/**
  * @brief 	SENSOR读寄存器
  * @ref 	RegAddress 寄存器地址，范围：参考SENSOR手册的寄存器描述
  * @return	读取寄存器的数据，范围：0x00~0xFF
  */
uint8_t SENSOR_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	
	I2C_GenerateSTART(I2C2, ENABLE);										//硬件I2C生成起始条件
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT);					//等待EV5
	
	I2C_Send7bitAddress(I2C2, SENSOR_ADDRESS, I2C_Direction_Transmitter);	//硬件I2C发送从机地址，方向为发送
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);	//等待EV6
	
	I2C_SendData(I2C2, RegAddress);											//硬件I2C发送寄存器地址
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED);				//等待EV8_2
	
	I2C_GenerateSTART(I2C2, ENABLE);										//硬件I2C生成重复起始条件
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT);					//等待EV5
	
	I2C_Send7bitAddress(I2C2, SENSOR_ADDRESS, I2C_Direction_Receiver);		//硬件I2C发送从机地址，方向为接收
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED);		//等待EV6
	
	I2C_AcknowledgeConfig(I2C2, DISABLE);									//在接收最后一个字节之前提前将应答失能
	I2C_GenerateSTOP(I2C2, ENABLE);											//在接收最后一个字节之前提前申请停止条件
	
	SENSOR_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED);				//等待EV7
	Data = I2C_ReceiveData(I2C2);											//接收数据寄存器
	
	I2C_AcknowledgeConfig(I2C2, ENABLE);									//将应答恢复为使能，为了不影响后续可能产生的读取多字节操作
	
	return Data;
}



/**
  * @brief 	：SENSOR I2C初始化
  * @ref 	：NULL
  * @return	：NULL
  */
void SENSOR_I2C_Init(void)
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);		//开启I2C2的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);								//将PB10和PB11引脚初始化为复用开漏输出
	
	/*I2C初始化*/
	I2C_InitTypeDef I2C_InitStructure;						
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;				//模式，选择为I2C模式
	I2C_InitStructure.I2C_ClockSpeed = 100000;				//时钟速度，选择为100KHz
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;		//时钟占空比，选择Tlow/Thigh = 2
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;				//应答，选择使能
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;	//应答地址，选择7位，从机模式下才有效
	I2C_InitStructure.I2C_OwnAddress1 = 0x00;				//自身地址，从机模式下才有效
	I2C_Init(I2C2, &I2C_InitStructure);						//将结构体变量交给I2C_Init，配置I2C2
	
	/*I2C使能*/
	I2C_Cmd(I2C2, ENABLE);									//使能I2C2，开始运行
}



///////////////////////*Data Transform*///////////////////////


/**
  * @brief 	激活SPI2、PB12写1 [NSS]
  * @ref 		bitvalue
  * @return	NULL
  */
void Cmos_SPI_W_SS(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_12,(BitAction)BitValue);
}

/**
  * @brief SPI2初始化，[STM32主机][CMOS主机][单工][mode0]
  * @ref 		NULL
  * @return NULL
  */
void CmoSPi_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,ENABLE);			//开启SPI2时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;				//推挽输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;							//NSS PB12
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;		//时钟浮空输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;							//MOSI 主机输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);	

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;		//时钟浮空输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;							//SCL
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);		
	
	//SPI配置
	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Slave;						//从机
	SPI_InitStructure.SPI_Direction=SPI_Direction_1Line_Rx;			//单工接收
	SPI_InitStructure.SPI_DataSize=SPI_DataSize_8b;					//8bit
	SPI_InitStructure.SPI_FirstBit=SPI_FirstBit_LSB;				//低位在前
	SPI_InitStructure.SPI_BaudRatePrescaler=SPI_BaudRatePrescaler_64;	//72M/128
	SPI_InitStructure.SPI_CPOL=SPI_CPOL_Low;								//空闲低电平
	SPI_InitStructure.SPI_CPHA=SPI_CPHA_1Edge;							//第一边缘采样(移入)
	SPI_InitStructure.SPI_NSS=SPI_NSS_Soft;									//多主机控制
	SPI_InitStructure.SPI_CRCPolynomial=7;									//校验？
	SPI_Init(SPI2,&SPI_InitStructure);
	SPI_Cmd(SPI2,ENABLE);	
		
	Cmos_SPI_W_SS(1);																				//关闭片选	
}

/**
  * @brief 	等待RNXE位，把SPI_DR里面的数据取出
  * @ref 		NULL
  * @return uint8_t SPI_DR
  */
uint8_t Cmos_SPI_Rx(void)
{
	while(SPI_I2S_GetFlagStatus(SPI2,SPI_I2S_FLAG_RXNE)!=SET);
	return SPI_I2S_ReceiveData(SPI2);
}



/**
  * @brief 	DMA初始化
  * @ref 		AddrA 原数组的首地址,AddrB 目的数组的首地址,Size 转运的数据大小（转运次数）
  * @return	NULL
  */
void MyDMA_Init(uint32_t AddrA, uint32_t AddrB, uint16_t Size)
{	
	/*开启时钟*/
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);						//开启DMA的时钟
	DMA_DeInit(DMA1_Channel4); 																		// SPI2_RX 对应 DMA1 通道 4
	/*DMA初始化*/
	DMA_InitTypeDef DMA_InitStructure;														//定义结构体变量
	DMA_InitStructure.DMA_PeripheralBaseAddr = AddrA;							//外设基地址，给定形参AddrA
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;	//外设数据宽度，选择字节
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;		//外设地址自增，选择使能
	DMA_InitStructure.DMA_MemoryBaseAddr = AddrB;									//存储器基地址，给定形参AddrB
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;			//存储器数据宽度，选择字节
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;					//存储器地址自增，选择使能
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;						//数据传输方向，选择由外设到存储器
	DMA_InitStructure.DMA_BufferSize = Size;								//转运的数据大小（转运次数）
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;							//模式，选择正常模式
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;							//存储器到存储器，选择使能
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;					//优先级，选择中等
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);							//将结构体变量交给DMA_Init，配置DMA1的通道1
	
	
	//传输完成后启动中断
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  // 配置抢占优先级
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         // 配置子优先级
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;            // 启用中断
  	NVIC_Init(&NVIC_InitStructure);
	DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);
	NVIC_EnableIRQ(DMA1_Channel4_IRQn);
	
	
	/*DMA使能*/
	DMA_Cmd(DMA1_Channel4, DISABLE);	//这里先不给使能，初始化后不会立刻工作，等后续调用Transfer后，再开始
}

/**
  * @brief 	DMA转运目标变化(双缓冲)
  * @ref 	转运目标地址
  * @return	NULL
  */

void DMA_Switch_Buffer(uint32_t Buffer)
{
	DMA1_Channel4->CMAR = Buffer;
}



/**
  * @brief 	双buffer的转换
  * @ref 		NULL
  * @return	完成转运的buffer地址
  */
uint8_t * Buffer_Swap(void)
{	
	if(V_current%2==0)		//偶数行为even
	{
		pixel_buffer = e_buffer;
		DMA1_Channel4->CMAR = (uint32_t)o_buffer;
		return e_buffer;
	}else					//奇数行为odd
	{
		pixel_buffer = o_buffer;
		DMA1_Channel4->CMAR = (uint32_t)e_buffer;
		return o_buffer;
	}
}


/**
  * @brief 	识别图像数据信息，自适应修改图像宽度，DMA转运次数
  * @ref 		NULL
  * @return	NULL
  */
void ImageData_Recog(void)
{
	Cmos_SPI_W_SS(0);																							//激活SPI2 片选	
	/*识别帧头、break退出*/	
	while(1)
	{		
		//识别第一个code
		//读到帧头后，读取长宽 
		sync_code = Cmos_SPI_Rx();
		if(sync_code==0xff)				//第一个FF
		{
			sync_code = Cmos_SPI_Rx();			
			if(sync_code==0xff)			//第二个FF
			{
				sync_code = Cmos_SPI_Rx();
				if(sync_code==0xff)		//第三个FF
				{
					sync_code = Cmos_SPI_Rx();
					if(sync_code==0x01)	//识别到帧头code后的处理code:01
					{							
							//Frame Start
							//识别帧数据量
							//OLED_ShowString(4,1,"Frame Start");							
							V_DataID = Cmos_SPI_Rx();
							Image_Width = Cmos_SPI_Rx();
							Image_Width |= (uint32_t)Cmos_SPI_Rx()<<8;
							Cmos_SPI_Rx();//Image_Height = Cmos_SPI_Rx();
							Cmos_SPI_Rx();//Image_Height |= (uint32_t)Cmos_SPI_Rx()<<8;
							break;													
					}					
				}
			}	
		}		
	}			
	DataSize = (Image_Width+12)*Image_Height+6;//DMA转运次数
	LCD_SetWindow(50,50,49+Image_Width,49+Image_Height);//LCD窗口
	total_pixels = Image_Width*Image_Height;
	
}

/**
  * @brief 	预览画面实现（图形数据从cmos到stm32(DMA1 通道4),再从stm32到显示器的数据流转）负责开头的帧头识别，后续交给中断函数
  * @ref 		NULL
  * @return	NULL
  */
void ImageData_Transfer(void)
{
	Cmos_SPI_W_SS(0);																							//激活SPI2 片选
	MyDMA_Init((uint32_t)&SPI2->DR,(uint32_t)e_buffer,DataSize);				//初始化DMA			开始的时候放在偶数行																							
	DMA_SetCurrDataCounter(DMA1_Channel4, DataSize);
	/*识别帧头、分发DMA任务、break退出*/	
	while(1)
	{		
		//识别第一个code
		//读到帧头后，读取长宽
		sync_code = Cmos_SPI_Rx();
		if(sync_code==0xff)				//第一个FF
		{
			sync_code = Cmos_SPI_Rx();			
			if(sync_code==0xff)			//第二个FF
			{
				sync_code = Cmos_SPI_Rx();
				if(sync_code==0xff)		//第三个FF
				{
					sync_code = Cmos_SPI_Rx();
					if(sync_code==0x01)	//识别到帧头code后的处理code:01
					{							
							//Frame Start							
							Cmos_SPI_Rx();// V_DataID = Cmos_SPI_Rx();
							Cmos_SPI_Rx();// Image_Width = Cmos_SPI_Rx();
							Cmos_SPI_Rx();// Image_Width |= (uint32_t)Cmos_SPI_Rx()<<8;
							Cmos_SPI_Rx();// Image_Height = Cmos_SPI_Rx();
							Cmos_SPI_Rx();// Image_Height |= (uint32_t)Cmos_SPI_Rx()<<8;													
							//DMA转运当前帧,数据转运次数为datasize									
							DMA_Cmd(DMA1_Channel4, ENABLE);															
							SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);							
							break;													//退出循环
					}				
				}
			}	
		}		
	}		
	V_current++;											//帧计数+1
}

/**
  * @brief 	中断，识别下一帧和分发DMA任务
  * @ref 		NULL
  * @return	NULL
  */
void DMA1_Channel4_IRQHandler(void)
{

	/**中断处理*/
	if (DMA_GetITStatus(DMA1_IT_TC4))  				// 检查传输完成中断
    {
        DMA_ClearITPendingBit(DMA1_IT_TC4);  		// 清除中断标志
		}	
	/**图像显示*/	
	Show_Image();
			
	/*分发DMA任务*/
	DMA_Cmd(DMA1_Channel4, DISABLE);
	Buffer_Swap();									//变换缓冲区
		
	DMA_SetCurrDataCounter(DMA1_Channel4, DataSize);
	//识别帧头、分发DMA任务、break退出	
	while(1)
	{		
		//识别第一个code
		sync_code = Cmos_SPI_Rx();
		if(sync_code==0xff)				//第一个FF
		{
			sync_code = Cmos_SPI_Rx();			
			if(sync_code==0xff)			//第二个FF
			{
				sync_code = Cmos_SPI_Rx();
				if(sync_code==0xff)		//第三个FF
				{
					sync_code = Cmos_SPI_Rx();
					if(sync_code==0x01)	//识别到帧头code后的处理code:01
					{							
							//Frame Start
							Cmos_SPI_Rx();// V_DataID = Cmos_SPI_Rx();
							Cmos_SPI_Rx();// Image_Width = Cmos_SPI_Rx();
							Cmos_SPI_Rx();// Image_Width |= (uint32_t)Cmos_SPI_Rx()<<8;
							Cmos_SPI_Rx();// Image_Height = Cmos_SPI_Rx();
							Cmos_SPI_Rx();// Image_Height |= (uint32_t)Cmos_SPI_Rx()<<8;	
							//DMA转运当前帧
							DMA_Cmd(DMA1_Channel4, ENABLE);															
							SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);
							break;													
					}					
				}
			}	
		}		
	}	
	V_current++;											
}

/**
  * @brief 	显示图像
  * @ref 	NULL
  * @return	NULL
  */
void Show_Image(void)
{
	//数据处理
	LCD_W_DC(1);
	LCD_SPI_Start();
	counter=12;
	//连续写入像素数据
	for(uint32_t i=0; i<total_pixels; i++) {		
		color16 = (pixel_buffer[i+counter]>>3)<<11|(pixel_buffer[i+counter]>>2)<< 5|pixel_buffer [i+counter]>> 3;
		LCD_SPI_SwapBytpe(color16 >> 8);  // 高字节
		LCD_SPI_SwapBytpe(color16 & 0xFF);// 低字节
		if((i+1)%128==0)counter+=12;
	}
	LCD_SPI_Stop();
}


/////////////////**Drive**/////////////////

/**
  * @brief 	SENSOR初始化
  * @ref 	NULL
  * @return	NULL
  */
void SENSOR_Init(SENSOR_InitTypeDef * InitStruture)
{
	XCLKInit();
	PWDInit();
	PWDSet(0);
	SENSOR_ADDRESS = (InitStruture->SENSOR_ADDRESS) & 0xFF;
	SENSOR_ID_ADDRESS = (InitStruture->SENSOR_ID_ADDRESS) & 0xFF;
	SENSOR_ID = (InitStruture->SENSOR_ID) & 0xFF;
	Image_Height = (InitStruture->Image_Height) & 0xFF;
	SENSOR_I2C_Init();			//初始化I2C
	CmoSPi_Init();				//初始化SPI2
	V_current =0;				//帧计数0
}

/**
  * @brief 	SENSOR获取ID号
  * @ref 		NULL
  * @return sensor的ID号
  */
 uint8_t SENSOR_GetID(void)
 {
	 return SENSOR_ReadReg(SENSOR_ID_ADDRESS);		//返回sensor ID
 }
 
 /**
  * @brief 	数据大小
  * @ref 		NULL
  * @return	DataSize
  */
uint16_t ImageData_Size(void)
{
		/*打印信息*/
		OLED_ShowNum(0, 20,DataSize,6,OLED_8X16);
		OLED_ShowNum(0, 40,Image_Width,4,OLED_8X16);
		OLED_ShowNum(40, 40,Image_Height,4,OLED_8X16);	
	return DataSize;
}
