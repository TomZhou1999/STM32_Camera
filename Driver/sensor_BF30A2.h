#ifndef __SENSOR_BF30A2_H__
#define __SENSOR_BF30A2_H__


void Sensor_BF30A2Init(void);
uint8_t BF30A2_GetID(void);
uint8_t BF30A2_ReadReg(uint8_t Reg);

#endif
