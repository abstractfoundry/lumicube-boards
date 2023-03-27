/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_SERVO_MODULE

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "afException.h"
#include "useful.h"
#include "afFieldProtocol.h"


typedef struct sservoDriver {
	TIM_HandleTypeDef * tim;
	uint32_t channel;
	uint16_t minPulseUsec;
	uint16_t maxPulseUsec;
	uint16_t currentAngleDegreeHundreth;
    uint16_t angle;
} servoDriver;

typedef struct sservoBoardDriver {
	servoDriver servo[NUM_SERVOS];
} servoBoardDriver;

servoBoardDriver servoDrv = {};



uint16_t angleToPulseUs(servoDriver * s, uint16_t angleDegreeHundreth) {
	return (s->minPulseUsec + ((angleDegreeHundreth * (s->maxPulseUsec - s->minPulseUsec)) / (180 * 100)));
}

void setServoAngle(servoDriver * s, uint16_t angleDegreeHundreth) {
	softAssert(angleDegreeHundreth <= (180 * 100), "Angle can't be more than 180 degrees");
	s->currentAngleDegreeHundreth = angleDegreeHundreth;
	__HAL_TIM_SET_COMPARE(s->tim, s->channel, angleToPulseUs(s, s->currentAngleDegreeHundreth));
	// printf("Setting angle to %d degrees\r\n", angleDegrees);
}

static void setServoAngleField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    uint32_t index = (startFieldIndex / 3);
    memcpy(&servoDrv.servo[index].angle, data, 2);
    setServoAngle(&servoDrv.servo[index], servoDrv.servo[index].angle);
}

#ifndef SERVOS_FIELDS_OFFSET
    #define SERVOS_FIELDS_OFFSET 0
#endif

static uint8_t * const componentName = (uint8_t *) "servos";
static sExtraMetaDataFields servoFieldMetaData = {
	2, {
		{SETTABLE, &(uint8_t){0}},
		{COMPONENT_NAME, componentName}
	}
};
sFieldInfoEntry servosFieldTable[] = {
    {&servoDrv.servo[0].angle,        "servo_0_angle",            SERVOS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[0].minPulseUsec, "servo_0_min_pulse",        SERVOS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[0].maxPulseUsec, "servo_0_max_pulse",        SERVOS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
#if (NUM_SERVOS > 1)
    {&servoDrv.servo[1].angle,        "servo_1_angle",        3 + SERVOS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[1].minPulseUsec, "servo_1_min_pulse",    3 + SERVOS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[1].maxPulseUsec, "servo_1_max_pulse",    3 + SERVOS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
#endif
#if (NUM_SERVOS > 2)
    {&servoDrv.servo[2].angle,        "servo_2_angle",      2*3 + SERVOS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[2].minPulseUsec, "servo_2_min_pulse",  2*3 + SERVOS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[2].maxPulseUsec, "servo_2_max_pulse",  2*3 + SERVOS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
#endif
#if (NUM_SERVOS > 3)
    {&servoDrv.servo[3].angle,        "servo_3_angle",      3*3 + SERVOS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[3].minPulseUsec, "servo_3_min_pulse",  3*3 + SERVOS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[3].maxPulseUsec, "servo_3_max_pulse",  3*3 + SERVOS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
#endif
#if (NUM_SERVOS > 4)
    {&servoDrv.servo[4].angle,        "servo_4_angle",      4*3 + SERVOS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[4].minPulseUsec, "servo_4_min_pulse",  4*3 + SERVOS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[4].maxPulseUsec, "servo_4_max_pulse",  4*3 + SERVOS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
#endif
#if (NUM_SERVOS > 5)
    {&servoDrv.servo[5].angle,        "servo_5_angle",      5*3 + SERVOS_FIELDS_OFFSET + 0, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[5].minPulseUsec, "servo_5_min_pulse",  5*3 + SERVOS_FIELDS_OFFSET + 1, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
    {&servoDrv.servo[5].maxPulseUsec, "servo_5_max_pulse",  5*3 + SERVOS_FIELDS_OFFSET + 2, 1, AF_FIELD_TYPE_UINT,      2,  0, &servoFieldMetaData, &setServoAngleField, NULL},
#endif

};
const uint32_t servosFieldTableSize = sizeof(servosFieldTable) / sizeof(sFieldInfoEntry);

// ======================== //


uint8_t selfTestServos() {
    // Just check that the buttons haven't been pressed
    // This should be called close to power on so no one should have pressed anything yet
    uint8_t failed = 0;
	return failed;
}

void initialiseServos(TIM_HandleTypeDef * tims[], uint32_t timChannels[]) {
    addComponentFieldTableToGlobalTable(servosFieldTable, servosFieldTableSize);
    
    for (uint8_t i=0; i<NUM_SERVOS; i++) {
        servoDrv.servo[i].minPulseUsec = 544;
        servoDrv.servo[i].maxPulseUsec = 2500;
        servoDrv.servo[i].tim = tims[i];
        servoDrv.servo[i].channel = timChannels[i];
        HAL_StatusTypeDef status = HAL_TIM_PWM_Start(tims[i], timChannels[i]);
		softAssert(status == HAL_OK, "Failed to start PWM");
    }
}

void loopServos(uint32_t ticks) {
    static uint32_t ticksSinceLastPublish = 0;
    // Publish every second
    if (ticks - ticksSinceLastPublish > 1000) {
        ticksSinceLastPublish = ticks;
        publishFieldsIfBelowBandwidth(SERVOS_FIELDS_OFFSET, SERVOS_FIELDS_OFFSET + NUM_SERVOS*3 - 1);
    }
}

#endif //ENABLE_SERVO_MODULE
