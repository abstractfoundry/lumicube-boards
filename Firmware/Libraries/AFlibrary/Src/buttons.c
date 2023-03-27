/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_BUTTON_MODULE

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "afException.h"
#include "useful.h"
#include "afFieldProtocol.h"

typedef struct {
    GPIO_TypeDef * gpioSection;
    uint16_t gpioPin;
    uint8_t pressed;
    uint32_t pressedCount;
} sButton;

typedef struct {
    TIM_HandleTypeDef * tim;
    sButton button[NUM_BUTTONS];
} sButtonBoardDriver;

sButtonBoardDriver buttonsDrv = {};

#ifndef BUTTONS_FIELDS_OFFSET
    #define BUTTONS_FIELDS_OFFSET 0
#endif

static uint8_t * const buttonName = (uint8_t *) "buttons";
static sExtraMetaDataFields buttonFieldMetaData = {
	2, {
		{SETTABLE, &(uint8_t){0}},
		{COMPONENT_NAME, buttonName}
	}
};
sFieldInfoEntry buttonsFieldTable[] = {
    {&buttonsDrv.button[0].pressed,      BUTTON_0_NAME "_pressed",            BUTTONS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_BOOLEAN,   1,  0, &buttonFieldMetaData, NULL, NULL},
    {&buttonsDrv.button[0].pressedCount, BUTTON_0_NAME "_pressed_count",      BUTTONS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      4,  0, &buttonFieldMetaData, NULL, NULL},
#if (NUM_BUTTONS > 1)
    {&buttonsDrv.button[1].pressed,      BUTTON_1_NAME "_pressed",            BUTTONS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_BOOLEAN,   1,  0, &buttonFieldMetaData, NULL, NULL},
    {&buttonsDrv.button[1].pressedCount, BUTTON_1_NAME "_pressed_count",      BUTTONS_FIELDS_OFFSET + 3, 1, AF_FIELD_TYPE_UINT,      4,  0, &buttonFieldMetaData, NULL, NULL},
#endif
#if (NUM_BUTTONS > 2)
    {&buttonsDrv.button[2].pressed,      BUTTON_2_NAME "_pressed",            BUTTONS_FIELDS_OFFSET + 4, 1, AF_FIELD_TYPE_BOOLEAN,   1,  0, &buttonFieldMetaData, NULL, NULL},
    {&buttonsDrv.button[2].pressedCount, BUTTON_2_NAME "_pressed_count",      BUTTONS_FIELDS_OFFSET + 5, 1, AF_FIELD_TYPE_UINT,      4,  0, &buttonFieldMetaData, NULL, NULL},
#endif
#if (NUM_BUTTONS > 3)
    {&buttonsDrv.button[3].pressed,      BUTTON_3_NAME "_pressed",            BUTTONS_FIELDS_OFFSET + 6, 1, AF_FIELD_TYPE_BOOLEAN,   1,  0, &buttonFieldMetaData, NULL, NULL},
    {&buttonsDrv.button[3].pressedCount, BUTTON_3_NAME "_pressed_count",      BUTTONS_FIELDS_OFFSET + 7, 1, AF_FIELD_TYPE_UINT,      4,  0, &buttonFieldMetaData, NULL, NULL},
#endif
#if (NUM_BUTTONS > 4)
    {&buttonsDrv.button[4].pressed,      BUTTON_4_NAME "_pressed",            BUTTONS_FIELDS_OFFSET + 8, 1, AF_FIELD_TYPE_BOOLEAN,   1,  0, &buttonFieldMetaData, NULL, NULL},
    {&buttonsDrv.button[4].pressedCount, BUTTON_4_NAME "_pressed_count",      BUTTONS_FIELDS_OFFSET + 9, 1, AF_FIELD_TYPE_UINT,      4,  0, &buttonFieldMetaData, NULL, NULL},
#endif
};
const uint32_t buttonsFieldTableSize = sizeof(buttonsFieldTable) / sizeof(sFieldInfoEntry);

static void publishButtonFields(uint16_t button) {
    publishFieldsIfBelowBandwidth(BUTTONS_FIELDS_OFFSET + 2*button, BUTTONS_FIELDS_OFFSET + 2*button + 1);
}

// ======================== //

#define DEBOUNCE_MSEC 10

static uint8_t getDebouncedButtonPressed(uint16_t * count, uint8_t rawPressed) {
    if (rawPressed) {
        if (*count >= DEBOUNCE_MSEC) {
            return 1;
        } else {
            (*count)++;
        }
    } else {
        *count = 0;
    }
    return 0;
    // *state = (*state << 1) | !rawPressed;
    // if ((*state & 0xFFF) == 0x000)
    //  return 1;
    // return 0;
}

static void checkButtons() {
    static uint16_t count[NUM_BUTTONS] = {};
    for (uint8_t i=0; i<NUM_BUTTONS; i++) {
        uint8_t pressed = getDebouncedButtonPressed(&count[i], (HAL_GPIO_ReadPin(buttonsDrv.button[i].gpioSection, buttonsDrv.button[i].gpioPin) == GPIO_PIN_SET));
        if (pressed && !buttonsDrv.button[i].pressed) {
            buttonsDrv.button[i].pressedCount++;
        }
        buttonsDrv.button[i].pressed = pressed;
    }
}

void button_TIM_PeriodElapsedCallback() {
    checkButtons();
}

uint8_t selfTestButtons() {
    // Just check that the buttons haven't been pressed
    // This should be called close to power on so no one should have pressed anything yet
    uint8_t failed = 0;
    for (uint8_t i=0; i<NUM_BUTTONS; i++) {
        failed |= (buttonsDrv.button[i].pressedCount > 0) || buttonsDrv.button[i].pressed;
    }
    softAssert(!failed, "Button self test failed");
	return failed;
}

void initialiseButtons(TIM_HandleTypeDef * tim, GPIO_TypeDef* buttonGpioSections[], uint16_t buttonGpioPins[]) {
    addComponentFieldTableToGlobalTable(buttonsFieldTable, buttonsFieldTableSize);
    buttonsDrv.tim = tim;
    for (uint8_t i=0; i<NUM_BUTTONS; i++) {
        buttonsDrv.button[i].gpioPin = buttonGpioPins[i];
        buttonsDrv.button[i].gpioSection = buttonGpioSections[i];
    }

    softAssert(HAL_TIM_Base_Start_IT(tim) == HAL_OK, "Failed");

    // HAL_Delay(100);
    // selfTestButtons();
}

void loopButtons(uint32_t ticks) {
    static uint32_t ticksSinceLastPublish = 0;
    // Publish any changes as soon as they happen
    static uint32_t lastPressedCount[NUM_BUTTONS] = {};
    for (uint8_t i=0; i<NUM_BUTTONS; i++) {
        if (buttonsDrv.button[i].pressedCount != lastPressedCount[i]) {
            lastPressedCount[i] = buttonsDrv.button[i].pressedCount;
            publishButtonFields(i);
        }
    }
    // Publish every second
    if (ticks - ticksSinceLastPublish > 1000) {
        ticksSinceLastPublish = ticks;
        publishFieldsIfBelowBandwidth(BUTTONS_FIELDS_OFFSET, BUTTONS_FIELDS_OFFSET + (2*NUM_BUTTONS) -1);
    }
}

#endif //ENABLE_BUTTON_MODULE
