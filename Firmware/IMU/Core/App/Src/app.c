/* Copyright (c) 2020 Abstract Foundry Limited */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "project.h"
#include "useful.h"
#include "afFieldProtocol.h"
#include "can.h"
#include "bno055.h"
#include "imu.h"
#include "debugComponent.h"


#include "main.h"

extern CAN_HandleTypeDef hcan;
#ifdef IMU_I2C
    extern I2C_HandleTypeDef hi2c1;
#else
    extern UART_HandleTypeDef huart2;
#endif
extern TIM_HandleTypeDef htim17;

char * getBoardName() {
    return "imu";
}
char * getBoardNodeName() {
    return "com.abstractfoundry.imu";
}

void setupCanHw() {
    hcan.Instance = CAN; // NOTE: canard library only works with CAN1
    HAL_CAN_MspInit(&hcan);
    return;


    // GPIO_InitTypeDef GPIO_InitStruct = {};

    // hcan.Instance = CAN; // NOTE: canard library only works with CAN1
    // __HAL_RCC_CAN1_CLK_ENABLE();
    // __HAL_RCC_GPIOA_CLK_ENABLE();

    // // Rx - same as HAL_CAN_MspInit
    // GPIO_InitStruct.Pin = GPIO_PIN_11;
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    // GPIO_InitStruct.Pull = GPIO_NOPULL;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    // GPIO_InitStruct.Alternate = GPIO_AF4_CAN;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // // Tx - different to HAL_CAN_MspInit
    // GPIO_InitStruct.Pin = GPIO_PIN_12;
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_OD; //GPIO_MODE_AF_PP; /* !! This is our difference - use it open drain mode !! */ ;
    // GPIO_InitStruct.Pull = GPIO_NOPULL;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    // GPIO_InitStruct.Alternate = GPIO_AF4_CAN;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

uint8_t boardTest() { // Called by can.c
    uint8_t errorCode = 0;
    errorCode |= testImu();
    return errorCode;
}

void setup() {
    bool initialised = false;
    while (!initialised) {
        CEXCEPTION_T e;
        Try {
            #ifdef IMU_I2C
                MX_I2C1_Init();
            #else
                MX_USART2_UART_Init();
            #endif
            initialiseImu(
                // NEED TO CHANGE IOC at the same time!

                // Latest - v0.11
                GPIOA, GPIO_PIN_0, // Reset
                GPIOA, GPIO_PIN_5, // Bootloader
                GPIOA, GPIO_PIN_6, // ps1
                GPIOA, GPIO_PIN_1, // interrupt
                GPIOA, GPIO_PIN_7, // bootloaderIndicator

                // //v0.3 - Needs special can setup
                // GPIOA, GPIO_PIN_6, // Reset
                // GPIOA, GPIO_PIN_4, // Bootloader
                // NULL, GPIO_PIN_0, // ps1
                // GPIOA, GPIO_PIN_7, // interrupt
                // GPIOA, GPIO_PIN_5, // bootloaderIndicator

                // // v0.1
                // GPIOA, GPIO_PIN_6, // Reset
                // GPIOA, GPIO_PIN_4, // Bootloader
                // NULL, GPIO_PIN_0, // ps1
                // GPIOA, GPIO_PIN_5, // interrupt
                // GPIOA, GPIO_PIN_7, // bootloaderIndicator
                #ifdef IMU_I2C
                    &hi2c1
                #else
                    &huart2
                #endif
            );
            initialiseDebug();
            setupCanHw();
            setupCan(false, NULL);
            softAssert(HAL_TIM_Base_Start_IT(&htim17) == HAL_OK, "Failed to start canTimer");
            boardTest();
            initialised = 1;
            
#ifdef TEST_SELF_CHECKS
            for (uint32_t i=0; i<10000; i++) {
                boardTest();
            }
            softAssert(0, "");
#endif
        } Catch(e) {}
    }
}

void loopBackground() {
    HAL_Delay(10);
}

// Called by timer every 1ms
void loopMain() {
    START_THREAD_EXCEPTION(MAIN_THREAD_ID);
    CEXCEPTION_T e;
    Try {
        uint32_t ticks = HAL_GetTick();
        loopUavCan();
        loopDebug(ticks);
        loopImu(ticks);
    } Catch(e) {}
    END_THREAD_EXCEPTION();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    static uint16_t count = 0;
    if (htim == &htim17) {
        loopRawCan();
        count++;
        if (count == 10) {
            // Every 1ms
        	count = 0;
            loopMain();
        }
    }
}

