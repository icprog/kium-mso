#ifndef GLOBAL_H
#define GLOBAL_H
#include "FreeRTOS.h"
#include "peripherals/mezonin/mezonin.h"
#include "constant.h"
extern mezonin mezonin_my[4];
//Message Actual_Message;
extern QueueHandle_t xCanQueue;


extern SemaphoreHandle_t xTWISemaphore;

#endif
