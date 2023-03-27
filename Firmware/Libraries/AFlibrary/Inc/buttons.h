/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_BUTTON_MODULE

void initialiseButtons(TIM_HandleTypeDef * tim, GPIO_TypeDef* buttonGpioSections[], uint16_t buttonGpioPins[]);
void loopButtons(uint32_t ticks);
void button_TIM_PeriodElapsedCallback();
uint8_t selfTestButtons();

#endif //ENABLE_BUTTON_MODULE