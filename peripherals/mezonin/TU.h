/*
 * TU.h
 *
 *  Created on: 25 сент. 2014 г.
 *      Author: maximus
 */

#ifndef TU_H_
#define TU_H_
#include "mezcommon.h"
//==========================парамеры канала, хранящиеся в EEPROM для ТU=======================
typedef struct _TU_Param
{
	uint32_t Mode;			// режим работы
	uint32_t TimeTU;			// время выдержки ТУ
//	uint8_t NumberTC;		// ТС концевиков
//	uint8_t ExtraTimeTU;	// Дополнительное время выдержки ТУ
	uint16_t CRC;			// контрольная сумма
} TU_Param;
//---------------структура канала ТU (тип mezonin)----------------------
typedef struct _TU_Channel
{
	uint8_t State;  	// состояние
	uint8_t Value; 		// значение канала (On или Off)
	uint32_t TickCount;  // время до отключения
	TU_Param Params;		// параметры из EEPROM
} TU_Channel;
//---------------определение типа для ТU (тип mezonin)------------------
typedef struct _TU_Value
{
	TU_Channel Channel[4]; // номер канала
	int32_t ID; // номер мезонина
} TU_Value;

extern TU_Value Mezonin_TU[4];

void Mez_TU_init(mezonin *MezStruct);
void WriteTUParams(uint8_t MezNum, int ChannelNumber, TU_Param* Params);
void Set_TUDefaultParams(uint8_t MezNum);
uint32_t Get_TUParams(TU_Value *TU_temp);
void TUTickHandler(void);
void Mez_TU_handler(mezonin *MezStruct);
#endif /* TU_H_ */
