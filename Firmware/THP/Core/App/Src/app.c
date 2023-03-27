/* Copyright (c) 2020 Abstract Foundry Limited */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "project.h"
#include "useful.h"
#include "afFieldProtocol.h"
#include "can.h"
#include "thp.h"
#include "debugComponent.h"

extern CAN_HandleTypeDef hcan;
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim17;
extern TIM_HandleTypeDef htim16;

char * getBoardName() {
	return "env_sensor";
}
char * getBoardNodeName() {
	return "com.abstractfoundry.BME280";
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim == &htim17) {
        loopRawCan();
    }
}

// CubeIde current:
// Run, 48mhz - 13.1 mA
// Run, 48mhz, can, i2c - 13.9 mA
// Run, 24mhz, can, i2c - 7.51 mA
// Run, 8mhz, can, i2c - 2.83 mA
// Sleep, 48mhz, can, i2c - 3.32 mA
// Sleep, 8mhz, can, i2c - 0.8 mA

void sleepFor1Sec() {
    // Stop all other interrupts
    hardAssert(HAL_TIM_Base_Stop(&htim17) == HAL_OK, "Failed to stop can timer");
    HAL_SuspendTick();
    hardAssert(HAL_TIM_Base_Start_IT(&htim16) == HAL_OK, "Failed to start wakeup timer");

    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI); //PWR_LOWPOWERREGULATOR_ON

    hardAssert(HAL_TIM_Base_Stop(&htim16) == HAL_OK, "Failed to stop can timer");
    HAL_ResumeTick();
    incrementTimestampUsec(1000000);
    hardAssert(HAL_TIM_Base_Start_IT(&htim17) == HAL_OK, "Failed to start can timer on wakeup");
}

void setupCanHw() {
    hcan.Instance = CAN; // NOTE: canard library only works with CAN1
    HAL_CAN_MspInit(&hcan);
    return;

    // GPIO_InitTypeDef GPIO_InitStruct = {};

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

uint8_t boardTest() {
    return testThp();
}

void setup() {}

bool initialised = 0;
void loop() {
//    static uint8_t count = 0;
    CEXCEPTION_T e;
    Try {
        if (!initialised) {
            initialiseThp(&hi2c1);
            initialiseDebug();
            setupCanHw();
            setupCan(false, NULL);
            softAssert(HAL_TIM_Base_Start_IT(&htim17) == HAL_OK, "Failed to start canTimer");
            initialised = 1;
        } else {
            // sleepFor1Sec();

            uint32_t ticks = HAL_GetTick();
            loopDebug(ticks);
            loopThp(ticks);
            for (uint8_t i=0; i<10; i++) {
                HAL_Delay(1);
                loopUavCan();
            }
            // while (!allCanFramesSent()) {
            //     loopUavCan();
            // }

            // count++;
            // if (count > 100) {
            //     count = 0;
            //     sleepFor1Sec();
            // }
        }

    } Catch(e) {}
}
