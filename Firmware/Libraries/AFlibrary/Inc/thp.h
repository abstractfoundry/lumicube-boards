/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_ENV_SENSOR_MODULE

void initialiseThp(I2C_HandleTypeDef * i2c);
void loopThp(uint32_t ticks);
uint8_t testThp();

#endif // ENABLE_ENV_SENSOR_MODULE
