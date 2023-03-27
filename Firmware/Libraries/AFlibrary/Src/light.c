/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_LIGHT_MODULE

// #include <stdio.h>
#include <stdlib.h>
// #include <math.h>
#include <string.h>
#include <stdint.h>

#include "apds9960I2C.h"
#include "useful.h"
#include "afException.h"
#include "afFieldProtocol.h"

#define DEFAULT_PROXIMITY_STRENGTH 5
#define GESTURE_PROXIMITY_ENTRY_THRESHOLD 40
#define GESTURE_PROXIMITY_EXIT_THRESHOLD 30
#define DEFAULT_GESTURE_DIMENSION LEFT_RIGHT_UP_DOWN
// #define DEFAULT_MODE COLOUR_PROXIMITY_GESTURE

typedef enum edirectionEnum { NONE, UP, DOWN, LEFT, RIGHT } directionEnum;
typedef enum eapds9960GestureDimensionEnum { LEFT_RIGHT_UP_DOWN, LEFT_RIGHT, UP_DOWN } gestureDimensionEnum;
typedef enum eapds9960ModeEnum { COLOUR_PROXIMITY_GESTURE, COLOUR, PROXIMITY, GESTURE, DISTANCE } apds9960ModeEnum;

typedef struct sapds9960Driver {
	uint8_t init;
    I2C_HandleTypeDef * i2c;

	apds9960ModeEnum mode;
	apds9960ModeEnum currentOperation;
	// Colour
	uint8_t colourSensitivity; // Changed by user and by driver
	uint8_t autoAdaptSensitivity;
	uint32_t ambientLight;
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	// Proximity/distance
	uint8_t proximityStrength; // Changed by user and by driver
	uint8_t autoAdaptStrength; // Changed by user and by driver
	uint32_t proximity;
	uint8_t withinProximity;
	uint32_t numTimesWithinProximity;
	// Gesture/Proximity
	uint32_t numGestures;
	directionEnum gesture;
	gestureDimensionEnum gestureDimension;
} apds9960Driver;

#define APDS9960_ID  0xAB

apds9960Driver apdsDrv = {};

typedef struct {
	char * name;
	uint8_t type;
	uint8_t size;
	uint16_t span;
	void * field;
} sFieldInfo;

// typedef enum eapds9960RequestEnum {
// 	CHANGE_MODE,
// 	SET_PROXIMITY_STRENGTH,
// 	SET_AUTOMATIC_PROXIMITY_STRENGTH,
// 	SET_COLOUR_SENSITIVITY,
// 	SET_AUTOMATIC_COLOUR_SENSITIVITY,
// 	SET_GESTURE_DIRECTION_SETTING,
// } apds9960RequestEnum;

// void serviceRequest(apds9960Driver * drv, apds9960RequestEnum request, uint8_t value) {
// 	switch(request) {
// 		case CHANGE_MODE:
// 			drv->mode = value;
// 			drv->currentOperation = value;
// 			break;
// 		case SET_AUTOMATIC_PROXIMITY_STRENGTH:
// 			drv->autoAdaptStrength = 1;
// 		case SET_PROXIMITY_STRENGTH:
// 			drv->autoAdaptStrength = 0;
// 			drv->proximityStrength = value;
// 			setProximityAndGestureStrength(&drv->proximityStrength, value);
// 			break;
// 		case SET_AUTOMATIC_COLOUR_SENSITIVITY:
// 			drv->autoAdaptSensitivity = 1;
// 		case SET_COLOUR_SENSITIVITY:
// 			drv->autoAdaptSensitivity = 0;
// 			setColourSensorSensitivity(&drv->colourSensitivity, value);
// 			break;
// 		case SET_GESTURE_DIRECTION_SETTING:
// 			setGestureDimension(value);
// 			drv->gestureDimension = value;
// 			break;
// 	}
// }

#ifndef LIGHT_FIELDS_OFFSET
    #define LIGHT_FIELDS_OFFSET 0
#endif

static uint8_t * const lightSensorName = (uint8_t *) "light_sensor";
static sExtraMetaDataFields lightFieldMetaData = {
	2, {
		{SETTABLE, &(uint8_t){0}},
		{COMPONENT_NAME, lightSensorName}
	}
};
sFieldInfoEntry lightFieldTable[] = {
    {&apdsDrv.ambientLight,             "ambient_light",               LIGHT_FIELDS_OFFSET + 0,  1,  AF_FIELD_TYPE_UINT,     4,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.red,                      "red",                         LIGHT_FIELDS_OFFSET + 1,  1,  AF_FIELD_TYPE_UINT,     4,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.green,                    "green",                       LIGHT_FIELDS_OFFSET + 2,  1,  AF_FIELD_TYPE_UINT,     4,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.blue,                     "blue",                        LIGHT_FIELDS_OFFSET + 3,  1,  AF_FIELD_TYPE_UINT,     4,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.gesture,                  "last_gesture",                LIGHT_FIELDS_OFFSET + 4,  1,  AF_FIELD_TYPE_ENUM,     1,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.numGestures,              "num_gestures",                LIGHT_FIELDS_OFFSET + 5,  1,  AF_FIELD_TYPE_UINT,     4,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.withinProximity,          "within_proximity",            LIGHT_FIELDS_OFFSET + 6,  1,  AF_FIELD_TYPE_BOOLEAN,  1,  0,  &lightFieldMetaData,  NULL,  NULL},
    {&apdsDrv.numTimesWithinProximity,  "num_times_within_proximity",  LIGHT_FIELDS_OFFSET + 7,  1,  AF_FIELD_TYPE_UINT,     4,  0,  &lightFieldMetaData,  NULL,  NULL},
};
const uint32_t lightFieldTableSize = sizeof(lightFieldTable) / sizeof(sFieldInfoEntry);


void publishAmbientLight() {
	publishFieldsIfBelowBandwidth(LIGHT_FIELDS_OFFSET + 0, LIGHT_FIELDS_OFFSET + 3);
}
void publishGesture() {
	publishFieldsIfBelowBandwidth(LIGHT_FIELDS_OFFSET + 4, LIGHT_FIELDS_OFFSET + 5);
}
void publishProximity() {
	publishFieldsIfBelowBandwidth(LIGHT_FIELDS_OFFSET + 6, LIGHT_FIELDS_OFFSET + 7);
}


//===============================//
// COLOUR

typedef struct tColourSensitivitySettings {
	uint8_t accumulationTime; // accumulationTime (256-accumulationTime)*2.78ms (255 => 2.78ms, 254 => 5.56ms, ... 0 => 711.68ms)
	uint8_t again; // again - 0 = 1x, 1 => 4x, 2 => 16x, 3 => 64x
} colourSensitivitySettings;
// Convert the accumulation time and gain of the colour sensor
// into a single setting that goes up in powers of 4 (i.e. 4^sensitivity)
#define MAX_COLOUR_SENSITIVITY 5
// NOTE: using an acc time of less than 44.5ms only gives us a faster reading
// it doesn't actually have a higher ceiling as the max count decreases as the acc time decreases
colourSensitivitySettings apds9960SensitivitySettings[MAX_COLOUR_SENSITIVITY+1] = {
	{240, 0 }, // 4^0 - acc time: 16  => 44.5ms,  maxCount: 1024*16
	{240, 1 }, // 4^1 - acc time: 16  => 44.5ms,  maxCount: 1024*16
	{240, 2 }, // 4^2 - acc time: 16  => 44.5ms,  maxCount: 1024*16
	{240, 3 }, // 4^3 - acc time: 16  => 44.5ms,  maxCount: 1024*16
	{192, 3 }, // 4^4 - acc time: 64  => 177.9ms, maxCount: 65535
	{0,   3 }  // 4^5 - acc time: 256 => 711.7ms, maxCount: 65535
};

void setColourSensorSensitivity(uint8_t * sensitivity, uint8_t newValue) {
	if (*sensitivity != newValue) {
		if (newValue > MAX_COLOUR_SENSITIVITY) {
//			printf("Invalid colour sensitivity, got %d, max %d\r\n", newValue, MAX_COLOUR_SENSITIVITY);
			Throw(GENERAL_EXCEPTION);
		}
		colourSensitivitySettings s = apds9960SensitivitySettings[newValue];
		i2cWriteField(apdsDrv.i2c, ATIME, s.accumulationTime);
		i2cWriteField(apdsDrv.i2c, CONTROL_AGAIN, s.again);
		*sensitivity = newValue;
	}
}

void enableColour() {
	// Could use caching to improve performance
	setEnableBits(apdsDrv.i2c, 1, 0, 0);
}

uint8_t colourReady() {
	return i2cReadField(apdsDrv.i2c, STATUS_AVALID);
}

// Try and adjust the gain and accumulation time to get the full dynamic range
// If it's dark increase gain to capture as much light as possible
// If it's bright decrease the gain to not saturate the counters
void getColourCountsAndAdjustSensitivity(uint32_t * ambientLight, uint32_t * red, uint32_t * green, uint32_t * blue, uint8_t * sensitivity, uint8_t adjustSensitivity) {
	uint16_t rawAmbientLight, rawRed, rawGreen, rawBlue;
	i2cReadARGBRegisters(apdsDrv.i2c, &rawAmbientLight, &rawRed, &rawGreen, &rawBlue);
	uint16_t factor = power(4, (MAX_COLOUR_SENSITIVITY - *sensitivity));
	*ambientLight = rawAmbientLight * factor;
	*red = rawRed * factor;
	*green = rawGreen * factor;
	*blue = rawBlue * factor;

	// Adjust the sensitivity for next time
	if (adjustSensitivity) {
		uint8_t newSensitivity = *sensitivity;
		// if (*ambientLight < 20 * power(4, 0)) {
		// 	// Very slow - only use if light is very low
		// 	newSensitivity = 5;
		// } else
		if (*ambientLight < (20 * power(4, 1))) {
			// Pretty slow - only use if light is very low
			newSensitivity = 4;
		} else if (*ambientLight < (2048 * power(4, 2))) {
			newSensitivity = 3;
		} else if (*ambientLight < (2048 * power(4, 3))) {
			newSensitivity = 2;
		} else if (*ambientLight < (2048 * power(4, 4))) {
			newSensitivity = 1;
		} else {
			newSensitivity = 0;
		}
		setColourSensorSensitivity(sensitivity, newSensitivity);
	}
}

//===============================//
// PROXIMITY

typedef struct tPoximityStrengthSettings {
	uint8_t pulseLength; // pulseLength - 0 = 4us, 1 = 8us, 2 = 16us, 3 = 32us
	uint8_t pulseCount; // Number of pulses 0 = 1, 1 = 2, ... 63 = 64
	uint8_t pgain; // pgain - 0 = 1x, 1 => 2x, 2 => 4x, 3 => 8x
	uint8_t ledDrive; // ledDrive - 0 = 100mA, 50mA, 25mA, 12.5mA
	uint8_t ledBoost; // ledBoost - 0 = 100% current, 1 = 150%, 2 = 200%, 3 = 300%
} poximityStrengthSettings;

#define MAX_PROXIMITY_STRENGTH 13
poximityStrengthSettings apds9960StrengthSettings[MAX_PROXIMITY_STRENGTH+1] = {
	// Found pulse length doesn't seem to affect it in a proportional way
	// so leave it at a fixed value
	{2, 0,  0, 3, 0}, // 2^0  - 12.5mA
	{2, 1,  0, 3, 0}, // 2^1  - 12.5mA
	{2, 3,  0, 3, 0}, // 2^2  - 12.5mA
	{2, 3,  1, 3, 0}, // 2^3  - 12.5mA
	{2, 7,  1, 3, 0}, // 2^4  - 12.5mA
	{2, 15, 1, 3, 0}, // 2^5  - 12.5mA
	{2, 15, 2, 3, 0}, // 2^6  - 12.5mA
	{2, 31, 2, 3, 0}, // 2^7  - 12.5mA
	{2, 63, 2, 3, 0}, // 2^8  - 12.5mA
	{2, 63, 2, 2, 0}, // 2^9  - 25mA
	{2, 63, 2, 1, 0}, // 2^10 - 50mA
	{2, 63, 2, 0, 0}, // 2^11 - 100mA
	{2, 63, 2, 0, 2}, // 2^12 - 200mA
	{2, 63, 2, 0, 3}, // 2^12 * 1.5 - 300 mA
};

// Convert all the different settings controlling the power and gain of the proximity sensor
// into a single setting that goes up in powers of 2 (i.e. 2^strength)
void setProximityOrGestureStrength(uint8_t strength, uint8_t gestureNotProximity) {
	if (strength > MAX_PROXIMITY_STRENGTH) {
//		printf("Invalid strength, got %x, max %d\r\n", strength, MAX_PROXIMITY_STRENGTH);
		return;
	}
	poximityStrengthSettings s = apds9960StrengthSettings[strength];
	// Gesture and proximity have the same settings but different fields
	i2cWriteField(apdsDrv.i2c, gestureNotProximity ? GPULSE_GPLEN : PPULSE_PPLEN, s.pulseLength);
	i2cWriteField(apdsDrv.i2c, gestureNotProximity ? GPULSE_GPULSE : PPULSE_PPULSE, s.pulseCount);
	i2cWriteField(apdsDrv.i2c, gestureNotProximity ? GCONF2_GLDRIVE : CONTROL_LDRIVE, s.ledDrive);
	i2cWriteField(apdsDrv.i2c, gestureNotProximity ? GCONF2_GGAIN : CONTROL_PGAIN, s.pgain);
	i2cWriteField(apdsDrv.i2c, CONFIG2_LED_BOOST, s.ledBoost);
}

// For simplicity keep these the same
void setProximityAndGestureStrength(uint8_t * strength, uint8_t newValue) {
	if (*strength != newValue) {
		*strength = newValue;
		setProximityOrGestureStrength(newValue, 0);
		setProximityOrGestureStrength(newValue, 1);
	}
}

void enableProximity() {
	setEnableBits(apdsDrv.i2c, 0,1,0);
}

uint8_t proximityReady() {
	return i2cReadField(apdsDrv.i2c, STATUS_PVALID);
}

uint8_t getRawProximity() {
	return i2cReadField(apdsDrv.i2c, PDATA);
}

//===============================//
// DISTANCE
// Try and adjust the gain and accumulation time to get the full dynamic range
// If it's dark increase gain to capture as much light as possible
// If it's bright decrease the gain to not saturate the counters
// The result is then normalised by factoring in the proximity strength

void getProximityAndAdjustStrength(uint32_t * proximity, uint8_t * strength, uint8_t autoAdaptStrength) {
	uint8_t rawProximity = getRawProximity();
	uint16_t factor = power(2, (MAX_PROXIMITY_STRENGTH - *strength));
	*proximity = rawProximity * factor;
	if (autoAdaptStrength) {
		// Keep the raw counts between 64-128
		uint8_t newStrength = 0;
		for (uint8_t i=0; i<=MAX_PROXIMITY_STRENGTH; i++) {
			if (*proximity < 128 * power(2,(MAX_PROXIMITY_STRENGTH - i))) {
				newStrength = i;
			}
		}
		setProximityAndGestureStrength(strength, newStrength);
	}
}

//===============================//
// GESTURE

void enableGesture() {
	// There are two modes for gesture capture
	// Automatic capture where the FIFOs are filled once the proximity threshold is exceeded
	// or manual capture where the FIFOs are filled as soon as we set the GEN bit
	// We use automatic mode which requires starting in proximity mode (with the GEN bit set)
	// It then requires reading when the GMODE is turned off to know when capture is finished
	setEnableBits(apdsDrv.i2c, 0,1,1);
}

void setGestureDimension(gestureDimensionEnum gestureDimension) {
	switch (gestureDimension) {
		case LEFT_RIGHT_UP_DOWN:
			i2cWriteField(apdsDrv.i2c, GCONF3_GDIMS, 0);
			break;
		case UP_DOWN:
			i2cWriteField(apdsDrv.i2c, GCONF3_GDIMS, 1);
			break;
		case LEFT_RIGHT:
			i2cWriteField(apdsDrv.i2c, GCONF3_GDIMS, 2);
			break;
	}
}

uint8_t gestureStarted() {
	return i2cReadField(apdsDrv.i2c, GCONF4_GMODE);
}

uint8_t gestureReady() {
	return (i2cReadField(apdsDrv.i2c, GSTATUS_GVALID) && !i2cReadField(apdsDrv.i2c, GCONF4_GMODE));
}

uint32_t getCentreOfMass(uint8_t * fifo, uint8_t fifoLevel) {
	uint32_t posWeightedSum = 0;
	uint32_t sum = 0;
	for (uint8_t i=0; i<fifoLevel; i++) {
		sum += fifo[i];
		posWeightedSum += (fifo[i] * (i + 1));
	}
	return ((posWeightedSum*100)/sum);
}

directionEnum gestureProcessing(uint8_t * upFifo, uint8_t * downFifo, uint8_t * leftFifo, uint8_t * rightFifo, uint8_t fifoLevel) {
	uint32_t upCoM = getCentreOfMass(upFifo, fifoLevel);
	uint32_t downCoM = getCentreOfMass(downFifo, fifoLevel);
	uint32_t leftCoM = getCentreOfMass(leftFifo, fifoLevel);
	uint32_t rightCoM = getCentreOfMass(rightFifo, fifoLevel);
	// If the right sensor detects something close first then it's actually a movement from
	// left to right, likewise for up and down
	int32_t rightOffset = rightCoM - leftCoM;
	int32_t downOffset = upCoM - downCoM;

	//uint32_t lrConfidence = (1000 * abs(rightCoM - leftCoM))/(rightCoM + leftCoM);
	//uint32_t udConfidence = (1000 * abs(upCoM - downCoM))/(upCoM + downCoM);
	// printf("fifoLevel %d, lrConfidence %ld, udConfidence %ld, dirConfidence %ld, LR:%ld, UD:%ld\r\n", fifoLevel, lrConfidence-udConfidence, lrConfidence, udConfidence, rightOffset, downOffset);
	directionEnum dir;
	if (abs(rightOffset) > abs(downOffset)) {
		if (rightOffset < 0) {
			dir = RIGHT;
		} else {
			dir = LEFT;
		}
	} else {
		if (downOffset < 0) {
			dir = UP;
		} else {
			dir = DOWN;
		}
	}
	return dir;
}

void readGesture(directionEnum * gesture, uint8_t * upFifo, uint8_t * downFifo, uint8_t * leftFifo, uint8_t * rightFifo, uint8_t * fifoLevel) {
	*fifoLevel = i2cReadField(apdsDrv.i2c, GFLVL);
	uint8_t dummyVariable;
	if (*fifoLevel > 3) {
		for (uint8_t i=0; i < *fifoLevel; i++) {
			i2cReadGestureFifo(apdsDrv.i2c, &upFifo[i], &downFifo[i], &leftFifo[i], &rightFifo[i]);
		}
		*gesture = gestureProcessing(&upFifo[0], &downFifo[0], &leftFifo[0], &rightFifo[0], *fifoLevel);
	} else {
		// Still need to read fifos to clear them down for next gesture
		for (uint8_t i=0; i < *fifoLevel; i++) {
			i2cReadGestureFifo(apdsDrv.i2c, &dummyVariable, &dummyVariable, &dummyVariable, &dummyVariable);
		}
		*gesture = NONE;
	}
}


void readAmbientLight() {
	enableColour();
	uint32_t startTick = HAL_GetTick();
	while (HAL_GetTick() - startTick < 250) {
		if (colourReady()) {
			getColourCountsAndAdjustSensitivity(
				&apdsDrv.ambientLight,
				&apdsDrv.red,
				&apdsDrv.green,
				&apdsDrv.blue,
				&apdsDrv.colourSensitivity,
				apdsDrv.autoAdaptSensitivity
			);
			// printf("Light:%6ld, R:%6ld, G:%6ld, B:%6ld, S:%d\n\r", drv->ambientLight, drv->red, drv->green, drv->blue, drv->colourSensitivity);
			break;
		}
	}	
}

void readGestureAndProximity() {
	// Proximity strength auto adaption is only valid for distance sensing
	if (apdsDrv.autoAdaptStrength) {
		apdsDrv.autoAdaptStrength = 0;
		setProximityAndGestureStrength(&apdsDrv.proximityStrength, DEFAULT_PROXIMITY_STRENGTH);
	}
	// In PROXIMITY mode We don't actually get the raw proximity value
	// We actually just start the gesture detection and see if starts detecting a gesture
	enableGesture();
	
	uint32_t startTick = HAL_GetTick();
	uint8_t gestureStart = 0;
	while (HAL_GetTick() - startTick < 500) {
			if (gestureStart == 0) {
				gestureStart = gestureStarted();
				if (gestureStart && apdsDrv.withinProximity == 0) {
					apdsDrv.withinProximity = 1;
					apdsDrv.numTimesWithinProximity++;
				}
			} else {
				uint8_t isGestureReady = gestureReady();
				if (isGestureReady) {
					uint8_t fifoLevel;
					uint8_t rawGestureUp[32];
					uint8_t rawGestureDown[32];
					uint8_t rawGestureLeft[32];
					uint8_t rawGestureRight[32];
					readGesture(
						&apdsDrv.gesture,
						&rawGestureUp[0],
						&rawGestureDown[0],
						&rawGestureLeft[0],
						&rawGestureRight[0],
						&fifoLevel
					);
					if (apdsDrv.gesture != NONE) {
						apdsDrv.numGestures++;
					}
					apdsDrv.withinProximity = 0;
					gestureStart = 0;
				}
			}
	}
}

//===============================//

uint8_t selfTestLight() {
	softAssert(apdsDrv.init, "Light sensor failed self test");
	return !apdsDrv.init;
}

void initialiseLight(I2C_HandleTypeDef * i2c) {
    addComponentFieldTableToGlobalTable(lightFieldTable, lightFieldTableSize);
    apdsDrv.i2c = i2c;
	
	uint8_t numTries = 0;
	while (numTries < 3 && !apdsDrv.init) {
		numTries++;
		CEXCEPTION_T e;
		Try {
			i2cWriteField(apdsDrv.i2c, ENABLE_PON, 1);
			uint8_t id = i2cReadField(apdsDrv.i2c, ID);
			hardAssert(id == 0xAB, "Invalid device ID");
			i2cWriteField(apdsDrv.i2c, GPENTH, GESTURE_PROXIMITY_ENTRY_THRESHOLD); // Entry proximity level
			i2cWriteField(apdsDrv.i2c, GEXTH, GESTURE_PROXIMITY_EXIT_THRESHOLD); // Exit proximity level
			setColourSensorSensitivity(&apdsDrv.colourSensitivity, 1);
			apdsDrv.autoAdaptSensitivity = 1;
			setProximityAndGestureStrength(&apdsDrv.proximityStrength, DEFAULT_PROXIMITY_STRENGTH);
			apdsDrv.autoAdaptStrength = 0;
			apdsDrv.gestureDimension = DEFAULT_GESTURE_DIMENSION;
			setGestureDimension(DEFAULT_GESTURE_DIMENSION);
			apdsDrv.init = 1;
		} Catch(e) {}
	}
	// drv->mode = DEFAULT_MODE;
	// drv->currentOperation = DEFAULT_MODE;
}

// All the work and I2C are in the background loop as the gesture calls take up to a second
void loopBackgroundLight() {
	if (apdsDrv.init) {
		readAmbientLight();
		readGestureAndProximity();
	}
}

// The CAN publishing is done in the main loop
void loopLight(uint32_t ticks) {
	static uint32_t nextTicks = 0;
	if (ticks > nextTicks && apdsDrv.init) {
		nextTicks = ticks + 100; // Publish every 0.1s
		publishAmbientLight();
		publishGesture();
		publishProximity();
	}		
}

#endif // ENABLE_LIGHT_MODULE

