/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_LIGHT_MODULE

#include <stdint.h>
uint8_t selfTestLight();
void initialiseLight(I2C_HandleTypeDef * i2c);
void loopBackgroundLight();
void loopLight();

#endif // ENABLE_LIGHT_MODULE
