/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_DIAL_MODULE

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "useful.h"
#include "afFieldProtocol.h"
#include "can.h"
#include "afException.h"


typedef struct sDialDriver {
    uint16_t rotation;
    uint16_t lastRotation;
    ADC_HandleTypeDef * adc;
} DialDriver;

static DialDriver dial = {};

static uint8_t zero = 0;
static sExtraMetaDataFields rotationMetaData = {2, {{SETTABLE, &zero}, {UNITS, (uint8_t *) "degrees"}}};

#ifndef DIAL_FIELDS_OFFSET
    #define DIAL_FIELDS_OFFSET 0
#endif

sFieldInfoEntry dialFieldTable[] = {
    { &dial.rotation, "rotation", DIAL_FIELDS_OFFSET +  0,  1,  AF_FIELD_TYPE_FLOAT,  2,  0,  &rotationMetaData, NULL, NULL},
};

const uint32_t dialFieldTableSize = sizeof(dialFieldTable) / sizeof(sFieldInfoEntry);

// ======================== //

uint8_t testDial() {
    uint8_t failed = 0;
    return failed;
}

void initialiseDial(ADC_HandleTypeDef * hadc) {
    addComponentFieldTableToGlobalTable(dialFieldTable, dialFieldTableSize);
    dial.adc = hadc;
}

void loopdial(uint32_t ticks) {
    static uint32_t nextTicks = 0;
    static uint32_t nextPublishTicks = 0;
    
    if (ticks > nextTicks) {
        nextTicks = ticks + 10; // Check ever 10ms
        softAssert(HAL_ADC_Start(dial.adc) == HAL_OK, "");
        softAssert(HAL_ADC_PollForConversion(dial.adc, 2) == HAL_OK, "");
        dial.rotation = HAL_ADC_GetValue(dial.adc);
        
        if ((dial.rotation != dial.lastRotation) || (ticks > nextPublishTicks)) {
            dial.lastRotation = dial.rotation;
            nextPublishTicks = ticks + 100;
            publishFieldsIfBelowBandwidth(DIAL_FIELDS_OFFSET, DIAL_FIELDS_OFFSET);
        }
    }
}

#endif //ENABLE_dial_MODULE
