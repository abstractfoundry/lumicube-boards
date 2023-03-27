/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_SPEAKER_MODULE

#include <stdint.h>
void initialiseSpeaker(TIM_HandleTypeDef * tim, DAC_HandleTypeDef * dac, GPIO_TypeDef * shutdownGPIOx, uint16_t shutdownGPIOPin, ADC_HandleTypeDef * testAdc);
void afterCanSetupSpeaker();
void loopSpeaker(uint32_t ticks);
uint8_t selfTestSpeaker();

#endif //ENABLE_SPEAKER_MODULE
