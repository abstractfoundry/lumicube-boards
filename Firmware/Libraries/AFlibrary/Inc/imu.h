/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_IMU_MODULE

void initialiseImu(GPIO_TypeDef* resetGpio, uint16_t resetGpioPin,
                    GPIO_TypeDef* bootloaderGpio, uint16_t bootloaderGpioPin, 
                    GPIO_TypeDef* ps1Gpio, uint16_t ps1GpioPin, 
                    GPIO_TypeDef* interruptGpio, uint16_t interruptGpioPin, 
                    GPIO_TypeDef* bootloadIndicatorGpio, uint16_t bootloadIndicatorGpioPin, 
                    #ifdef IMU_I2C
                        I2C_HandleTypeDef * i2c
                    #else
                        UART_HandleTypeDef * uart
                    #endif
                    );

void loopImu();
uint8_t testImu();

#endif //ENABLE_IMU_MODULE
