/*
 FreeRTOS V8.0.1 - Copyright (C) 2014 Real Time Engineers Ltd.
 All rights reserved

 VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

 ***************************************************************************
 *                                                                       *
 *    FreeRTOS provides completely free yet professionally developed,    *
 *    robust, strictly quality controlled, supported, and cross          *
 *    platform software that has become a de facto standard.             *
 *                                                                       *
 *    Help yourself get started quickly and support the FreeRTOS         *
 *    project by purchasing a FreeRTOS tutorial book, reference          *
 *    manual, or both from: http://www.FreeRTOS.org/Documentation        *
 *                                                                       *
 *    Thank you!                                                         *
 *                                                                       *
 ***************************************************************************

 This file is part of the FreeRTOS distribution.

 FreeRTOS is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License (version 2) as published by the
 Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

 >>!   NOTE: The modification to the GPL is included to allow you to     !<<
 >>!   distribute a combined work that includes FreeRTOS without being   !<<
 >>!   obliged to provide the source code for proprietary components     !<<
 >>!   outside of the FreeRTOS kernel.                                   !<<

 FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  Full license text is available from the following
 link: http://www.freertos.org/a00114.html

 1 tab == 4 spaces!

 ***************************************************************************
 *                                                                       *
 *    Having a problem?  Start by reading the FAQ "My application does   *
 *    not run, what could be wrong?"                                     *
 *                                                                       *
 *    http://www.FreeRTOS.org/FAQHelp.html                               *
 *                                                                       *
 ***************************************************************************

 http://www.FreeRTOS.org - Documentation, books, training, latest versions,
 license and Real Time Engineers Ltd. contact details.

 http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
 including FreeRTOS+Trace - an indispensable productivity tool, a DOS
 compatible FAT file system, and our tiny thread aware UDP/IP stack.

 http://www.OpenRTOS.com - Real Time Engineers ltd license FreeRTOS to High
 Integrity Systems to sell under the OpenRTOS brand.  Low cost OpenRTOS
 licenses offer ticketed support, indemnification and middleware.

 http://www.SafeRTOS.com - High Integrity Systems also provide a safety
 engineered and independently SIL3 certified version for use in safety and
 mission critical applications that require provable dependability.

 1 tab == 4 spaces!
 */

/*
 NOTE : Tasks run in System mode and the scheduler runs in Supervisor mode.
 The processor MUST be in supervisor mode when vTaskStartScheduler is
 called.  The demo applications included in the FreeRTOS.org download switch
 to supervisor mode prior to main being called.  If you are not using one of
 these demo application projects then ensure Supervisor mode is used.
 */

/* Standard includes. */

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
/* Demo application includes. */
#include "ParTest/partest.h"
#include "constant.h"
#include "MSO.h"

#include "peripherals/twi/twi.h"
#include "peripherals/PCA9532/PCA9532.h"
#include "peripherals/MSO_functions/MSO_functions.h"
#include "peripherals/can/can.h"
#include "peripherals/mezonin/mezonin.h"
#include "peripherals/spi/spi.h"
#include "peripherals/pio/pio_config.h"
#include "peripherals/aic/aic.h"
#include <math.h>

extern void TWI_ISR(void) __attribute__((naked));
extern void CAN0_ISR(void) __attribute__((naked));
extern void CAN1_ISR(void) __attribute__((naked));

/* Priorities for the demo application tasks. */
#define mainUIP_PRIORITY					( tskIDLE_PRIORITY + 2 )
#define mainUSB_PRIORITY					( tskIDLE_PRIORITY + 2 )
#define mainBLOCK_Q_PRIORITY				( tskIDLE_PRIORITY + 1 )
#define mainFLASH_PRIORITY                  ( tskIDLE_PRIORITY + 2 )
#define mainGEN_QUEUE_TASK_PRIORITY			( tskIDLE_PRIORITY ) 

#define mainUIP_TASK_STACK_SIZE_MAX		( configMINIMAL_STACK_SIZE * 10 )
#define mainUIP_TASK_STACK_SIZE_MED		( configMINIMAL_STACK_SIZE * 3 )
#define mainUIP_TASK_STACK_SIZE_MIN		( configMINIMAL_STACK_SIZE )

//volatile uint8_t MSO.Address;

SemaphoreHandle_t xTWISemaphore;
SemaphoreHandle_t xSPISemaphore;
QueueHandle_t xCanQueue, xMezQueue, xMezTUQueue;

CanTransfer canTransfer0;
CanTransfer canTransfer1;
CanTransfer canTransfer_new1;
CanTransfer canTransfer_new0;

mezonin mezonin_my[4];

TaskHandle_t xMezHandle;

TT_Value Mezonin_TT[4];
TC_Value Mezonin_TC[4];
TU_Value Mezonin_TU[4];
TR_Value Mezonin_TR[4];
TI_Value Mezonin_TI[4];

int32_t tt[100];
int32_t c = 0;

uint32_t TY_state = 0x00;
uint32_t Prev_SPI_RDR_value = 0x00;
uint32_t Cur_SPI_RDR_value = 0x00;

unsigned char RedLeds;
unsigned char GreenLeds;
unsigned char GreenBlink;
/*-----------------------------------------------------------*/

static void prvSetupHardware(void);

void PCA9532_init(void)
{
    TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_PSC0 | PCA9532_AUTO_INCR), 1);
    //PSC0 period 1 second
    TWI_WriteByte(AT91C_BASE_TWI, 0x97);
    Wait_THR_Ready();
    //PWM0 duty cycle 50%
    TWI_WriteByte(AT91C_BASE_TWI, 0x80);
    Wait_THR_Ready();
    //PSC1 frequency 2 Hz
    TWI_WriteByte(AT91C_BASE_TWI, 0x4B);
    Wait_THR_Ready();
    //PWM1 duty cycle 50%
    TWI_WriteByte(AT91C_BASE_TWI, 0x80);
    Wait_THR_Ready();
    //Switch off red leds
    TWI_WriteByte(AT91C_BASE_TWI, (LED_OFF(0) | LED_OFF(1) | LED_OFF(2) | LED_OFF(3)));
    Wait_THR_Ready();
    //Switch off green leds
    TWI_WriteByte(AT91C_BASE_TWI, (LED_OFF(0) | LED_OFF(1) | LED_OFF(2) | LED_OFF(3)));
    Wait_THR_Ready();
    TWI_Stop(AT91C_BASE_TWI);
    Wait_TXCOMP();
    AT91C_BASE_TWI->TWI_RHR;

}
//------------------------------------------//*//------------------------------------------------
void CanHandler(void *p)
{
    TickType_t xLastWakeTime;
//	uint32_t identifier;
    uint32_t Type, Priority, Channel_Num, Param;
    int32_t a;
    Mez_Value test23;
    Message Recieve_Message, Send_Message;

    // Initialize the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();
//	CAN_Init(1000, AT91C_CAN_MIDE | MSO.Address << PosAdress);
    InitCanRX(AT91C_CAN_MIDE | MSO.Address << PosAdress);
    //Setup receiving MailBox by MSO address
//	identifier = AT91C_CAN_MIDE | MSO.Address << PosAdress;
    for (;;) {
        // Wait for the next cycle.
        if (xQueueReceive(xCanQueue, &Recieve_Message, portMAX_DELAY)) { // We have CAN message
            vParTestToggleLED(0);
            // CAN ID parsing
            Type = Recieve_Message.real_Identifier >> PosType & MaskType;
            Priority = Recieve_Message.real_Identifier >> PosPriority & MaskPriority;
            Channel_Num = Recieve_Message.real_Identifier >> PosChannel & MaskChannel;
            Param = Recieve_Message.real_Identifier & MaskParam;
            Send_Message.canID = Recieve_Message.canID;

            switch (Priority) {
                case priority_V:
                    if (MSO.Mode == MSO_MODE_OFF)
                        break;
//					Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.Mode = mode_calib;
                    switch (Param) {
                        case ParamCalib0:
                            Mez_TT_Calib(&mezonin_my[Channel_Num / 4], Channel_Num % 4, 1);
                            break;
                        case ParamCalib20:
                            Mez_TT_Calib(&mezonin_my[Channel_Num / 4], Channel_Num % 4, 2);
                            break;
                        case ParamWrite:
                            Mez_TT_Calib(&mezonin_my[Channel_Num / 4], Channel_Num % 4, 3);
//							Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.Mode = mode_ok;
                            break;
                        case ParamCalibEnd:
                            Mez_TT_Calib(&mezonin_my[Channel_Num / 4], Channel_Num % 4, 4);
//							Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.Mode = mode_ok;
                            break;
                    }

                    break;

                case priority_W:
                    Send_Message.real_Identifier = AT91C_CAN_MIDE | priority_N << PosPriority | Type << PosType | MSO.Address << PosAdress
                            | Channel_Num << PosChannel | Param;
                    switch (Type) {
                        case identifier_TI:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case 0:
                                    Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].CountTI = Recieve_Message.data_low_reg;
                                    break;
                                case 2:
                                    Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].Params.CoeFf = *((float *) &(Recieve_Message.data_low_reg));
                                    Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].Params.CRC = Crc16(
                                            (uint8_t *) &(Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].Params), offsetof(TI_Param, CRC));
                                    WriteTIParams(Channel_Num / 4, Channel_Num % 4, &Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].Params);
                                    break;
                            }
                            break;
                        case identifier_TR:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case 0:
                                    test23.ID = Channel_Num / 4;
                                    test23.Channel = Channel_Num % 4;
                                    test23.fValue = *((float *) &(Recieve_Message.data_low_reg));
                                    if (mezonin_my[Channel_Num / 4].TPQueue != 0) {
                                        if (xQueueSend(mezonin_my[Channel_Num / 4].TPQueue, &test23, (TickType_t ) 0)) {

                                        }
                                    }
                                    break;
                            }
                            break;
                        case identifier_TU:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case 0:
                                    test23.ID = Channel_Num / 4;
                                    test23.Channel = Channel_Num % 4;
                                    test23.ui32Value = Recieve_Message.data_low_reg;
                                    if (mezonin_my[Channel_Num / 4].TUQueue != 0) {
                                        if (xQueueSend(mezonin_my[Channel_Num / 4].TUQueue, &test23, (TickType_t ) 0)) {

                                        }
                                    }
                                    break;
                            }
                            break;
                        case identifier_ParamTU:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case 2:
                                    Mezonin_TU[Channel_Num / 4].Channel[Channel_Num % 4].Params.TimeTU = Recieve_Message.data_low_reg;
                                    Mezonin_TU[Channel_Num / 4].Channel[Channel_Num % 4].Params.CRC = Crc16(
                                            (uint8_t *) &(Mezonin_TU[Channel_Num / 4].Channel[Channel_Num % 4].Params), offsetof(TU_Param, CRC));
                                    Send_Message.data_low_reg = Recieve_Message.data_low_reg;
                                    Send_Message.data_high_reg = 0;
                                    CAN_Write(&Send_Message);

                                    WriteTUParams(Channel_Num / 4, Channel_Num % 4, &Mezonin_TU[Channel_Num / 4].Channel[Channel_Num % 4].Params);
                            }
                            break;

                        case identifier_Level:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case ParamWrite: {
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.CRC = Crc16(
                                            (uint8_t *) &(Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels), offsetof(TT_Level, CRC));

                                    WriteTTLevels(Channel_Num / 4, Channel_Num % 4, &Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels);
                                }
                                    break;
                                case ParamMinWarn:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Min_W_Level = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamMaxWarn:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Max_W_Level = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamMinAlarm:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Min_A_Level = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamMaxAlarm:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Max_A_Level = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamSens:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Sense = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                            }
                            break;

                        case identifier_Coeff:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case ParamWrite: {
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.CRC = Crc16(
                                            (unsigned char *) &(Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs),
                                            sizeof(TT_Coeff) - sizeof(unsigned short) - 2);
                                    WriteTTCoeffs(Channel_Num / 4, Channel_Num % 4, &Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs);
                                    /*if (xTWISemaphore != NULL) {
                                     if (xSemaphoreTake( xTWISemaphore, ( TickType_t ) 10 ) == pdTRUE) {
                                     a = MEZ_memory_Write(Channel_Num / 4, Channel_Num % 4 + PageCoeff, 0x00, sizeof(TT_Coeff),
                                     (unsigned char *) &Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs);
                                     vTaskDelayUntil(&xLastWakeTime, 10);
                                     xSemaphoreGive(xTWISemaphore);
                                     }
                                     }*/
                                }
                                    break;
                                case ParamKmin:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.k_min = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamKmax:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.k_max = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamPmin:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.p_min = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamPmax:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.p_max = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                            }
                            break;

                        case identifier_ParamTT:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case ParamWrite: {
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.CRC = Crc16(
                                            (uint8_t *) &(Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params), offsetof(TT_Param, CRC));
                                    WriteTTParams(Channel_Num / 4, Channel_Num % 4, &Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params);

                                }
                                    break;
                                case ParamMode:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.Mode = Recieve_Message.data_low_reg;
                                    break;
                                case ParamTime:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MeasTime = Recieve_Message.data_low_reg;
                                    SetTTTime(&(Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4]));
                                    break;
                                case ParamMinD:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MinD = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamMaxD:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MaxD = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamMinFV:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MinF = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamMaxFV:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MaxF = *((float *) &(Recieve_Message.data_low_reg));
                                    break;
                                case ParamExtK:
                                    Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.ExtK = *((float *) &(Recieve_Message.data_low_reg));
                                    SetTTExtK(&(Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4]));
                                    break;
                            }
                            break;
                        case identifier_ParamTC:
                            if (MSO.Mode == MSO_MODE_OFF)
                                break;
                            switch (Param) {
                                case ParamWrite: // запись в ЕЕПРОМ!!!!!!!
                                    Mezonin_TC[Channel_Num / 4].Channel[Channel_Num % 4].Params.CRC = Crc16(
                                            (unsigned char *) &(Mezonin_TC[Channel_Num / 4].Channel[Channel_Num % 4].Params),
                                            4/*sizeof (TC_Param) - sizeof(unsigned short)*/);
                                    if (xTWISemaphore != NULL) {
                                        if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY) == pdTRUE) {
                                            a = MEZ_memory_Write(Channel_Num / 4, Channel_Num % 4 + 1, 0x00, sizeof(TC_Param),
                                                    (unsigned char *) &Mezonin_TC[Channel_Num / 4].Channel[Channel_Num % 4].Params);
                                            vTaskDelayUntil(&xLastWakeTime, 10);
                                            xSemaphoreGive(xTWISemaphore);
                                        }
                                    }
                                    break;
                                case ParamMode:
                                    Mezonin_TC[Channel_Num / 4].Channel[Channel_Num % 4].Params.Mode = Recieve_Message.data_low_reg;
                                    break;
                            }
                            break;
                        case identifier_ModeMSO:
                            switch (Param) {
                                case 0:
                                    MSO.Mode = Recieve_Message.data_low_reg;
                                    break;
                            }
                            break;
                    }
                    break;

                case priority_R: {
                    Send_Message.real_Identifier = AT91C_CAN_MIDE | priority_N << PosPriority | Type << PosType | MSO.Address << PosAdress
                            | Channel_Num << PosChannel | Param; // составление идентификатора для посылки
                    //identifier = AT91C_CAN_MIDE | priority_N << PosPriority | Type << PosType | MSO.Address << PosAdress | Channel_Num << PosChannel | Param; // составление идентификатора для посылки
//					canID = Recieve_Message.canID;	// номер CAN0
//					FillCanPacket(&canTransfer1, canID, 3, AT91C_CAN_MOT_TX | AT91C_CAN_PRIOR, 0x00000000, identifier); // Заполнение структуры CanTransfer с расширенным идентификатором

                    switch (Type) {
                        case identifier_TI:
                            switch (Param) {
                                case 0:
                                    Send_Message.data_low_reg = Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].CountTI;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case 1:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].Value;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case 2:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TI[Channel_Num / 4].Channel[Channel_Num % 4].Params.CoeFf;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }
                            break;
                        case identifier_TR:
                            switch (Param) {
                                case 0:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TR[Channel_Num / 4].Channel.flDAC;

                                    break;
                            }
                            break;
                        case identifier_TT:
                            switch (Param) {
                                case ParamFV:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Value;
                                    Send_Message.data_high_reg = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].State;
                                    break;
                            }

                            break;
                        case identifier_TU:
                            switch (Param) {
                                case ParamTU:
                                    Send_Message.data_low_reg = Mezonin_TU[Channel_Num / 4].Channel[Channel_Num % 4].Value;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }

                            break;
                        case identifier_TC:
                            switch (Param) {
                                case ParamTC:
                                    Send_Message.data_low_reg = Mezonin_TC[Channel_Num / 4].Channel[Channel_Num % 4].Value;
                                    Send_Message.data_high_reg = Mezonin_TC[Channel_Num / 4].Channel[Channel_Num % 4].State;
                                    break;
                            }

                            break;
                        case identifier_ParamTU:
                            switch (Param) {
                                case ParamTime:
                                    *((uint32_t *) &(Send_Message.data_low_reg)) = Mezonin_TU[Channel_Num / 4].Channel[Channel_Num % 4].Params.TimeTU;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }
                            break;
                        case identifier_ParamTT:
                            switch (Param) {
                                case ParamMode:
                                    *((int32_t *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.Mode;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamTime:
                                    *((int32_t *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MeasTime;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMinD:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MinD;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMaxD:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MaxD;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMinFV:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MinF;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMaxFV:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.MaxF;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamExtK:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Params.ExtK;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }
                            break;

                        case identifier_Level:
                            switch (Param) {
                                case ParamMinWarn:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Min_W_Level;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMaxWarn:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Max_W_Level;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMinAlarm:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Min_A_Level;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamMaxAlarm:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Max_A_Level;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamSens:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Levels.Sense;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }
                            break;

                        case identifier_Coeff:
                            switch (Param) {
                                case ParamKmin:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.k_min;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamKmax:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.k_max;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamPmin:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.p_min;
                                    Send_Message.data_high_reg = 0;
                                    break;
                                case ParamPmax:
                                    *((float *) &(Send_Message.data_low_reg)) = Mezonin_TT[Channel_Num / 4].Channel[Channel_Num % 4].Coeffs.p_max;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }
                            break;
                        case identifier_ModeMSO:
                            switch (Param) {
                                case 0:
                                    Send_Message.data_low_reg = MSO.Mode;
                                    Send_Message.data_high_reg = 0;
                                    break;
                            }
                            break;
                    }
                    CAN_Write(&Send_Message);
                }
                    break;
                case priority_N:

                    break;
            }
        }
    }
}
//------------------------------------------//*//------------------------------------------------
void LedBlinkTask(void *p)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = *((uint32_t *) p);					//1000;

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vParTestToggleLED(1);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

}
//------------------------------------------//*//------------------------------------------------
void get_TY_state(void)
{

    Cur_SPI_RDR_value = (AT91C_BASE_SPI0->SPI_RDR & 0xFFFF);

    TY_state = Cur_SPI_RDR_value >> 1;

    TY_state |= Prev_SPI_RDR_value << 7;

    TY_state &= 0xFF;

    Prev_SPI_RDR_value = Cur_SPI_RDR_value;
}
//------------------------------------------//*//------------------------------------------------
void Mez_TC_Task(void *p)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 200;
    int32_t Mez_num;

    xLastWakeTime = xTaskGetTickCount();

    Mez_num = (int32_t) p;
    for (;;) {
        Mez_TC_handler(&mezonin_my[Mez_num]);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
//------------------------------------------//*//------------------------------------------------
void Mez_TU_Task(void *p)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 200;
    int32_t Mez_num;

    xLastWakeTime = xTaskGetTickCount();

    Mez_num = (int32_t) p;
    for (;;) {
        Mez_TU_handler(&mezonin_my[Mez_num]);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
//------------------------------------------//*//------------------------------------------------
void Mez_TT_Task(void *p)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 200;
    int32_t Mez_Num;
    xLastWakeTime = xTaskGetTickCount();
    Mez_Num = (int32_t) p;
    for (;;) {
        Mez_TT_handler(&mezonin_my[Mez_Num]);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
//------------------------------------------//*//-----------------------------------------------
void Mez_TP_Task(void *p)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 200;
    int32_t Mez_num;

    xLastWakeTime = xTaskGetTickCount();

    Mez_num = (int32_t) p;
    for (;;) {
        Mez_TP_handler(&mezonin_my[Mez_num]);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

}
//------------------------------------------//*//------------------------------------------------
void Mez_TI_Task(void *p)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 1000;
    int32_t Mez_num;

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();
    Mez_num = (int32_t) p;
    for (;;) {
        if (xSPISemaphore != NULL) {
            if (xSemaphoreTake( xSPISemaphore, portMAX_DELAY ) == pdTRUE) {
                Mez_TI_handler(&mezonin_my[Mez_num]);
                xSemaphoreGive(xSPISemaphore);
            }
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // Perform action here.
    }
}

//------------------------------------------//*//------------------------------------------------

void MezValue(void *p)
{
    Mez_Value Mez_V;
    InitCanTX();

    for (;;) {
        if (xMezQueue != 0) {
            if (xQueueReceive(xMezQueue, &Mez_V, portMAX_DELAY)) {

                switch (mezonin_my[Mez_V.ID].Mez_Type) {
                    case Mez_TC:
                        TCValueHandler(&Mez_V);

                        break;

                    case Mez_TU:
                        break;

                    case Mez_TT:
                        TTValueHandler(&Mez_V);

                        break;

                    case Mez_TR:

                        break;

                    case Mez_TI:
                        TIValueHandler(&Mez_V);
                        break;

                    case Mez_NOT:

                        break;
                }

            }
        }
    }
}
//------------------------------------------//*//------------------------------------------------
void Work_task(void *p)
{
    int state = MSO_MODE_OFF;
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        if ((MSO.Mode == MSO_MODE_ON) && (state == MSO_MODE_OFF)) {
            if (xTWISemaphore != NULL) {
                if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                    TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
                    TWI_WriteByte(AT91C_BASE_TWI, RedLeds);
                    Wait_THR_Ready();
                    TWI_WriteByte(AT91C_BASE_TWI, GreenLeds);
                    Wait_THR_Ready();
                    TWI_Stop(AT91C_BASE_TWI);
                    Wait_TXCOMP();
                    AT91C_BASE_TWI->TWI_RHR;
                    xSemaphoreGive(xTWISemaphore);
                }
            }
            state = MSO_MODE_ON;
        } else if ((MSO.Mode == MSO_MODE_OFF) && (state == MSO_MODE_ON)) {
            if (xTWISemaphore != NULL) {
                if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                    TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
                    TWI_WriteByte(AT91C_BASE_TWI, RedLeds);
                    Wait_THR_Ready();
                    TWI_WriteByte(AT91C_BASE_TWI, GreenBlink);
                    Wait_THR_Ready();
                    TWI_Stop(AT91C_BASE_TWI);
                    Wait_TXCOMP();
                    AT91C_BASE_TWI->TWI_RHR;
                    xSemaphoreGive(xTWISemaphore);
                }
            }
            state = MSO_MODE_OFF;
        }
    }
    vTaskDelayUntil(&xLastWakeTime, 1000);
}
//------------------------------------------//*//------------------------------------------------
void MezRec(void *p) // распознование типа мезонина
{
    RedLeds = 0;
    GreenLeds = 0;
    GreenBlink = 0;
    int32_t i;
    signed char a;
    if (xTWISemaphore != NULL) {
        if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
            MSO.Address = Switch8_Read(); // Определение адреса МСО
        }
        xSemaphoreGive(xTWISemaphore);
    }

    for (i = 0; i < 4; i++) {
        Mezonin_TU[i].ID = i;
        Mezonin_TT[i].ID = i;
        Mezonin_TC[i].ID = i;
        Mezonin_TR[i].ID = i;
        Mezonin_TI[i].ID = i;
        if (xTWISemaphore != NULL) {
            if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                mezonin_my[i].Mez_Type = Mez_Recognition(i); // Определение типа мезонина
                xSemaphoreGive(xTWISemaphore);
            }
        }

        Mez_init(mezonin_my[i].Mez_Type, &mezonin_my[i]);

        switch (i) {
            case 0:
                a = '0';
                break;

            case 1:
                a = '1';
                break;

            case 2:
                a = '2';
                break;

            case 3:
                a = '3';
                break;
        }

        switch (mezonin_my[i].Mez_Type) {
            case Mez_TC:
                GreenLeds |= LED_ON(i);
                GreenBlink |= LED_PWM0(i);
                if ((xTWISemaphore != NULL )) {
                    if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY) == pdTRUE) {
                        if (Get_TCParams(&Mezonin_TC[i]) == 0) {
                            xTaskCreate(Mez_TC_Task, "Mez_TC_Task" + a, mainUIP_TASK_STACK_SIZE_MED, (void * )i, mainUIP_PRIORITY, NULL);
                        } else {
                            RedLeds |= LED_PWM1(i);
                        }
                        xSemaphoreGive(xTWISemaphore);
                    }
                }
                break;

            case Mez_TU:
                GreenLeds |= LED_ON(i);
                GreenBlink |= LED_PWM0(i);
                if ((xTWISemaphore != NULL )) {
                    if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY) == pdTRUE) {
                        if (Get_TUParams(&Mezonin_TU[i]) == 0) {
                            xTaskCreate(Mez_TU_Task, "Mez_TU" + a, mainUIP_TASK_STACK_SIZE_MIN, (void * )i, mainUIP_PRIORITY, NULL);
                        } else {
                            RedLeds |= LED_PWM1(i);
                        }
                        xSemaphoreGive(xTWISemaphore);
                    }
                }
                break;

            case Mez_TT:
                GreenLeds |= LED_ON(i);
                GreenBlink |= LED_PWM0(i);
                if ((xTWISemaphore != NULL )) {
                    if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                        if ((Get_TTParams(&Mezonin_TT[i]) || Get_TTCoeffs(&Mezonin_TT[i]) || Get_TTLevels(&Mezonin_TT[i])) == 0) {
                            xTaskCreate(Mez_TT_Task, "MEZ_TT_Task" + a, mainUIP_TASK_STACK_SIZE_MED, (void * )i, mainUIP_PRIORITY, NULL);
                        } else {
                            RedLeds |= LED_PWM1(i);
                        }
                        xSemaphoreGive(xTWISemaphore);
                    }
                }

                break;

            case Mez_TR:
                GreenLeds |= LED_ON(i);
                GreenBlink |= LED_PWM0(i);
                if ((xTWISemaphore != NULL )) {
                    if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                        xTaskCreate(Mez_TP_Task, "Mez_TP" + a, mainUIP_TASK_STACK_SIZE_MED, (void * )i, mainUIP_PRIORITY, NULL);
                        xSemaphoreGive(xTWISemaphore);
                    }
                }
                break;

            case Mez_TI:
                GreenLeds |= LED_ON(i);
                GreenBlink |= LED_PWM0(i);
                if ((xTWISemaphore != NULL )) {
                    if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY) == pdTRUE) {
                        if (Get_TIParams(&Mezonin_TI[i]) == 0) {
                            xTaskCreate(Mez_TI_Task, "MEZ_TI_Task" + a, mainUIP_TASK_STACK_SIZE_MED, (void * )i, mainUIP_PRIORITY, NULL);
                        } else {
                            RedLeds |= LED_PWM1(i);
                        }
                        xSemaphoreGive(xTWISemaphore);
                    }
                }
                break;

            case Mez_NOT:
                GreenLeds |= LED_OFF(i);
                break;

            default:
                RedLeds |= LED_ON(i);
                break;
        }
        if (xTWISemaphore != NULL) {
            if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
                TWI_WriteByte(AT91C_BASE_TWI, RedLeds);
                Wait_THR_Ready();
                TWI_WriteByte(AT91C_BASE_TWI, GreenBlink);
                Wait_THR_Ready();
                TWI_Stop(AT91C_BASE_TWI);
                Wait_TXCOMP();
                AT91C_BASE_TWI->TWI_RHR;
                xSemaphoreGive(xTWISemaphore);
            }
        }
    }
    xTaskCreate(Work_task, "Work_task", mainUIP_TASK_STACK_SIZE_MED, NULL, mainUIP_PRIORITY, NULL);
    xTaskCreate(CanHandler, "CanHandler", mainUIP_TASK_STACK_SIZE_MED, NULL, mainUIP_PRIORITY, NULL);

    vTaskDelete(*((TaskHandle_t *) p));
}
void MezSetDefaultConfig(void * p)
{
    uint8_t MezType;
    uint32_t Freq;
    Freq = 100;
    if (xTWISemaphore != NULL) {
        if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY) == pdTRUE) {
            MezType = Switch8_Read(); // Get Mez type to set
        }
        xSemaphoreGive(xTWISemaphore);
    }
    Mez_SetType(0, MezType);
    Mez_SetType(1, MezType);
    Mez_SetType(2, MezType);
    Mez_SetType(3, MezType);
    switch (MezType) {
        case Mez_TC:
            Set_TCDefaultParams(0);
            Set_TCDefaultParams(1);
            Set_TCDefaultParams(2);
            Set_TCDefaultParams(3);
            break;
        case Mez_TU:
            Set_TUDefaultParams(0);
            Set_TUDefaultParams(1);
            Set_TUDefaultParams(2);
            Set_TUDefaultParams(3);
            break;
        case Mez_TT:
            Set_TTDefaultParams(0);
            Set_TTDefaultParams(1);
            Set_TTDefaultParams(2);
            Set_TTDefaultParams(3);
            break;
        case Mez_TI:
            Set_TIDefaultParams(0);
            Set_TIDefaultParams(1);
            Set_TIDefaultParams(2);
            Set_TIDefaultParams(3);
            break;
    }
    xTaskCreate(LedBlinkTask, "LedBlink", mainUIP_TASK_STACK_SIZE_MIN, &Freq, mainUIP_PRIORITY, NULL);
    vTaskDelete(*((TaskHandle_t *) p));
}
//------------------------------------------//*//------------------------------------------------
void TestLedFlash()
{
    const int delay = 500;
    AT91C_BASE_PMC->PMC_PCDR = 1 << AT91C_ID_TWI;
    AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, I2CDATA | I2CCLOCK);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CDATA | I2CCLOCK);
    AT91F_PIO_CfgOutput(AT91C_BASE_PIOB, WP | LED_0 | LED_1);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, WP | LED_0 | LED_1);
    AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, I2CCSE_1 | I2CCSE_2 | I2CCSE_3 | I2CCSE_4);
    AT91F_PIO_CfgOutput(AT91C_BASE_PIOB, A1_0 | A1_1 | A2_0 | A2_1 | A3_0 | A3_1 | A4_0 | A4_1);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_1 | I2CCSE_2 | I2CCSE_3 | I2CCSE_4);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_0 | A1_1 | A2_0 | A2_1 | A3_0 | A3_1 | A4_0 | A4_1);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_1);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_1);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_2);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_2);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_3);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_3);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_4);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_4);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A1_0);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_0);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A1_1);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_1);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A2_0);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A2_0);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A2_1);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A2_1);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A3_0);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A3_0);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A3_1);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A3_1);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A4_0);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A4_0);

    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A4_1);
    vTaskDelay(delay);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A4_1);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, WP);
    vTaskDelay(delay);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, WP);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCLOCK);
    vTaskDelay(delay);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCLOCK);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CDATA);
    vTaskDelay(delay);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CDATA);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_0);
    vTaskDelay(delay);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_0);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_1);
    vTaskDelay(delay);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_1);

    TWI_Pin_config();
    //vTaskDelay(500);
    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_TWI;

    if (xTWISemaphore != NULL) {
        if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(0));
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(1));
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(2));
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(3));
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(0));
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(1));
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(2));
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, LED_ON(3));
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            TWI_StartWrite(AT91C_BASE_TWI, (unsigned char) PCA9532_address, (PCA9532_LS0 | PCA9532_AUTO_INCR), 1);
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_WriteByte(AT91C_BASE_TWI, 0);
            Wait_THR_Ready();
            TWI_Stop(AT91C_BASE_TWI);
            Wait_TXCOMP();
            AT91C_BASE_TWI->TWI_RHR;
            vTaskDelay(delay);

            xSemaphoreGive(xTWISemaphore);
        }
    }

}
//------------------------------------------//*//------------------------------------------------
#define   SendNumer     (unsigned int) 256

void TestShowLeds()
{
    if ((AT91C_BASE_PIOB->PIO_PDSR) & OVF_1) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A1_0);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_0);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & OVF_2) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A2_0);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A2_0);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & OVF_3) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A3_0);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A3_0);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & OVF_4) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A4_0);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A4_0);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & BRK_1) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A1_1);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_1);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & BRK_2) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A2_1);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A2_1);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & BRK_3) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A3_1);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A3_1);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & BRK_4) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, A4_1);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A4_1);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & MDATA_1) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_1);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_1);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & MDATA_2) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_2);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_2);
    }
    if ((AT91C_BASE_PIOB->PIO_PDSR) & MDATA_3) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_3);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_3);
    }
    if ((AT91C_BASE_PIOA->PIO_PDSR) & MDATA_4) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOA, I2CCSE_4);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_4);
    }
}

void TestSPI(void)
{
    unsigned int SPI0_configuration, CS_configuration;
    unsigned short BufToWrite[SendNumer ];
    unsigned short BufToRead[SendNumer ];
    unsigned int DataCounter = 0;

    for (unsigned int i = 0; i < SendNumer ; i++) {
        BufToWrite[i] = 0xAA00 + i;
        BufToRead[i] = 0xFFFF;
    }

    SPI_Disable(AT91C_BASE_SPI0);

    AT91F_PIO_CfgInput(AT91C_BASE_PIOB, OVF_1 | OVF_2 | OVF_3 | OVF_4 | BRK_1 | BRK_2 | BRK_3 | BRK_4 | MDATA_1 | MDATA_2 | MDATA_3);
    AT91F_PIO_CfgInput(AT91C_BASE_PIOA, MDATA_4);

    AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, I2CCSE_1 | I2CCSE_2 | I2CCSE_3 | I2CCSE_4 | FIN_1 | FIN_2 | FIN_3 | FIN_4);
    AT91F_PIO_CfgOutput(AT91C_BASE_PIOB, A1_0 | A1_1 | A2_0 | A2_1 | A3_0 | A3_1 | A4_0 | A4_1);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, I2CCSE_1 | I2CCSE_2 | I2CCSE_3 | I2CCSE_4);
    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_0 | A1_1 | A2_0 | A2_1 | A3_0 | A3_1 | A4_0 | A4_1);
    AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, CS_1 | CS_2 | CS_3 | CS_4 | CLK);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, CS_1 | CS_2 | CS_3 | CS_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, CS_1);

    TestShowLeds();
    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, CS_1 | CS_2 | CS_3 | CS_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, CS_2);

    TestShowLeds();

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, CS_1 | CS_2 | CS_3 | CS_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, CS_3);

    TestShowLeds();

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, CS_1 | CS_2 | CS_3 | CS_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, CS_4);

    TestShowLeds();

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, CS_1 | CS_2 | CS_3 | CS_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, CLK);

    TestShowLeds();

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, CLK);

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, FIN_1 | FIN_2 | FIN_3 | FIN_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, FIN_1);

    TestShowLeds();

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, FIN_1 | FIN_2 | FIN_3 | FIN_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, FIN_2);

    TestShowLeds();

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, FIN_1 | FIN_2 | FIN_3 | FIN_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, FIN_3);

    TestShowLeds();

    vTaskDelay(400);

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, FIN_1 | FIN_2 | FIN_3 | FIN_4);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOA, FIN_4);

    TestShowLeds();

    AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, FIN_1 | FIN_2 | FIN_3 | FIN_4);


    AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, A1_0 | A2_0 | A3_0 | A4_0);

    AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, MISO | MOSI | CLK, 0);

    CS_configuration = AT91C_SPI_BITS_16 | 0x30 << 8;

    SPI0_configuration = AT91C_SPI_MSTR | AT91C_SPI_MODFDIS | AT91C_SPI_PS_FIXED | 0x00 << 16;
    SPI_Configure(AT91C_BASE_SPI0, AT91C_ID_SPI0, SPI0_configuration);
    SPI_ConfigureNPCS(AT91C_BASE_SPI0, 0, CS_configuration);

    SPI_Enable(AT91C_BASE_SPI0);
    if (xSPISemaphore != NULL) {
        if (xSemaphoreTake( xSPISemaphore, portMAX_DELAY ) == pdTRUE) {
            for (unsigned int i = 0; i < SendNumer ; i++) {
                SPI_Write(AT91C_BASE_SPI0, 0, BufToWrite[i]);
                BufToRead[i] = SPI_Read(AT91C_BASE_SPI0);
            }
            xSemaphoreGive(xSPISemaphore);
        }
    }
//  AT91C_BASE_PMC->PMC_PCDR = 1 << AT91C_ID_SPI0;

    for (int i = 0; i < SendNumer ; i++) {
        if (BufToRead[i] == BufToWrite[i])
            DataCounter++;
    }

    if (DataCounter == SendNumer) {
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_0);
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_1);
    } else {
        AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_0);
        AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_1);
    }
}
void TestCAN()
{
    uint32_t ChannelNumber;
    uint32_t ID;
    uint32_t Count;
    Message Recieve_Message, Send_Message;
    ID = MAKE_MSG_ID(0, 0, 0, 0);
    xQueueReset(xCanQueue);

    for (int i = 0; i < 256; i++) {
        SendCanMessage(ID, i, i);
        if (xQueueReceive(xCanQueue, &Recieve_Message, 1000)) {
            if ((Recieve_Message.data_low_reg == i) && (Recieve_Message.data_high_reg == i)) {
                AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_0);
                AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_1);
            } else {
                AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_0);
                AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_1);
                vTaskDelay(1000);
                break;
            }
        } else {
            AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_0);
            AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_1);
            vTaskDelay(1000);
            break;
        }
        if (xQueueReceive(xCanQueue, &Recieve_Message, 1000)) {
            if ((Recieve_Message.data_low_reg == i) && (Recieve_Message.data_high_reg == i)) {
                AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_0);
                AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_1);
            } else {
                AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_0);
                AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_1);
                vTaskDelay(1000);
                break;
            }
        } else {
            AT91F_PIO_ClearOutput(AT91C_BASE_PIOB, LED_0);
            AT91F_PIO_SetOutput(AT91C_BASE_PIOB, LED_1);
            vTaskDelay(1000);
            break;
        }
    }

}
//------------------------------------------//*//------------------------------------------------
void TestTask(void * p)
{
    //const TickType_t xFrequency = 100;
    //TickType_t xLastWakeTime;
    //xLastWakeTime = xTaskGetTickCount();
    int test;
    MSO.Address = 1;
    InitCanRX(AT91C_CAN_MIDE | MSO.Address << PosAdress);
    InitCanTX();
    for (;;) {
        if (xTWISemaphore != NULL) {
            if (xSemaphoreTake( xTWISemaphore, portMAX_DELAY ) == pdTRUE) {
                test = Switch8_Read(); // Определение адреса МСО
            }
            xSemaphoreGive(xTWISemaphore);
        }
        switch (test) {
            case 0:
                TestLedFlash();
                break;
            case 1:
                TestSPI();
                break;
            case 2:
                TestCAN();
                break;
        }
        vTaskDelay(500);
        //vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
//------------------------------------------//*//------------------------------------------------
void prvSetupSPIMaster(AT91S_SPI *spi)
{
    uint32_t SPI0_configuration;

    AT91F_PIO_CfgOutput(AT91C_BASE_PIOB, LED_0);

    SPI_Pin_config();

    SPI0_configuration = AT91C_SPI_MSTR | AT91C_SPI_MODFDIS | AT91C_SPI_PS_VARIABLE | 0x00 << 16;
    SPI_Configure(AT91C_BASE_SPI0, AT91C_ID_SPI0, SPI0_configuration);

    SPI_Enable(AT91C_BASE_SPI0);
}
/*
 * Starts all the other tasks, then starts the scheduler.
 */
int main(void)
{
    uint32_t Freq;
    Mez_PreInit(&mezonin_my[0], &mezonin_my[1], &mezonin_my[2], &mezonin_my[3]);

    prvSetupHardware();
    prvSetupSPIMaster(AT91C_BASE_SPI0);
    vSemaphoreCreateBinary(xTWISemaphore);
    vSemaphoreCreateBinary(xSPISemaphore);
    xCanQueue = xQueueCreate(64, sizeof(Message));
    xMezQueue = xQueueCreate(32, sizeof(Mez_Value));
    xMezTUQueue = xQueueCreate(16, sizeof(Mez_Value));
    CAN_Init(1000);
    MSO.Mode = Switch2_Read();
    switch (MSO.Mode) {
        case MSO_MODE_OFF:
            Freq = 1000;
            //CAN_Init(1000, AT91C_CAN_MIDE | MSO.Address << PosAdress);
            xTaskCreate(LedBlinkTask, "LedBlink", mainUIP_TASK_STACK_SIZE_MIN, &Freq, mainUIP_PRIORITY, NULL);
            xTaskCreate(MezRec, "Recognition", mainUIP_TASK_STACK_SIZE_MED, &xMezHandle, mainUIP_PRIORITY, &xMezHandle);
            xTaskCreate(MezValue, "Value", mainUIP_TASK_STACK_SIZE_MED, NULL, mainUIP_PRIORITY, NULL);
            break;
        case MSO_MODE_MEZ_INIT:
            Freq = 100;
            //xTaskCreate( LedBlinkTask, "LedBlink", mainUIP_TASK_STACK_SIZE_MIN, &Freq, mainUIP_PRIORITY, NULL);
            xTaskCreate(MezSetDefaultConfig, "DefaultConfig", mainUIP_TASK_STACK_SIZE_MED*4, &xMezHandle, mainUIP_PRIORITY, &xMezHandle);
            //MEZ_memory_Read(MezNum, 0x00, 0x00, 1, &Mez_Type);
            break;
        case MSO_MODE_TEST:
            xTaskCreate(TestTask, "TestTask", mainUIP_TASK_STACK_SIZE_MED*4, &xMezHandle, mainUIP_PRIORITY, &xMezHandle);
            break;

    }

    /* Start the scheduler.

     NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
     The processor MUST be in supervisor mode when vTaskStartScheduler is
     called.  The demo applications included in the FreeRTOS.org download switch
     to supervisor mode prior to main being called.  If you are not using one of
     these demo application projects then ensure Supervisor mode is used here. */

    vTaskStartScheduler();

    /* We should never get here as control is now taken by the scheduler. */
    return 0;
}
/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
    uint32_t i;
    portDISABLE_INTERRUPTS();

    /* When using the JTAG debugger the hardware is not always initialised to
     the correct default state.  This line just ensures that this does not
     cause all interrupts to be masked at the start. */

    AT91C_BASE_AIC->AIC_EOICR = 0;

    /* Most setup is performed by the low level init function called from the
     startup asm file. */

    /* Enable the peripheral clock. */

    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOA;
    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOB;

    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_TWI;

    AT91F_PIO_CfgOutput(AT91C_BASE_PIOB, WP);
    AT91F_PIO_SetOutput(AT91C_BASE_PIOB, WP);

    for (i = 0; i < 4; i++) {
        AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, (1 << (4 + i)));
        AT91F_PIO_SetOutput(AT91C_BASE_PIOA, (1 << (4 + i)));
    }

    TWI_Pin_config();
    TWI_Configure(AT91C_BASE_TWI, TWCK, MCK);
    AIC_ConfigureIT(AT91C_ID_TWI, 0, TWI_ISR);
    AIC_EnableIT(AT91C_ID_TWI);

    AIC_DisableIT(mezonin_my[0].TC_ID);	// запрет прерываний TimerCounter
    AIC_DisableIT(mezonin_my[1].TC_ID);	// запрет прерываний TimerCounter
    AIC_DisableIT(mezonin_my[2].TC_ID);	// запрет прерываний TimerCounter
    AIC_DisableIT(mezonin_my[3].TC_ID);	// запрет прерываний TimerCounter

    PCA9532_init(); // Моргалка передними лампочками

    AIC_ConfigureIT(AT91C_ID_CAN0, AT91C_AIC_PRIOR_HIGHEST, CAN0_ISR);
    AIC_ConfigureIT(AT91C_ID_CAN1, AT91C_AIC_PRIOR_HIGHEST, CAN1_ISR);

    vParTestInitialise();
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
    TTTickHandler();
    TUTickHandler();
}

