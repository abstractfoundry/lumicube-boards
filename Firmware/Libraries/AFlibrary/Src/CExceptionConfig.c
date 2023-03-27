/* Copyright (c) 2020 Abstract Foundry Limited */

#include "CExceptionConfig.h"
#include "stdint.h"

uint8_t threadStack[CEXCEPTION_NUM_ID] = {BACKGROUND_THREAD_ID, 0, 0, 0};
uint8_t threadStackIndex = 0;
