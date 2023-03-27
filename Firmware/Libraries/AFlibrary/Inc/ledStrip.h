/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_LED_STRIP_MODULE

#include <stdint.h>

void initialiseLedStrip(TIM_HandleTypeDef * tim, TIM_HandleTypeDef * tim2, uint32_t timChannel, GPIO_TypeDef* testGpioX, uint16_t testGpioPin);
void afterCanSetupLedStrip();
void loopLedStrip(uint32_t ticks);
uint8_t selfTestLedStrip();
void ledStripSpreadSpectrumTimerCallback();
void ledStrip_TIM_PWM_PulseFinishedCallback();
void ledStrip_TIM_PWM_PulseFinishedHalfCpltCallback();

#endif // ENABLE_LED_STRIP
