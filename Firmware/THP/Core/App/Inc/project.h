/* Copyright (c) 2021 Abstract Foundry Limited */

#ifndef APP_INC_PROJECT_H_
#define APP_INC_PROJECT_H_

#define ENABLE_ENV_SENSOR_MODULE
#define F0
#define ASSERT_BUFFER_SIZE 50
//#define NUM_CANARD_MEMORY_POOL_BYTES 2048

// #define PRODUCTION

// Shouldn't really go here but these file names are chip specific
// and it's a pain to have ifdefs in all common c and h files
#include "stm32f0xx.h" // GPIO defs
#include "stm32f0xx_hal.h" // Get tick, delay
#include "stm32f0xx_hal_tim.h"

#endif /* APP_INC_PROJECT_H_ */
