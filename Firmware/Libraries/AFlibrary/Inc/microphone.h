/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_MICROPHONE_MODULE

#include <stdint.h>

void initialiseMicrophone(I2S_HandleTypeDef * i2s);
void afterCanSetupMicrophone();
void loopMicrophone(uint32_t ticks);
void microphone_SPI_RxHalfCpltCallback ();
void microphone_SPI_RxCpltCallback ();
uint8_t selfTestMicrophone();

#endif // ENABLE_MICROPHONE_MODULE
