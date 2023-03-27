/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_LED_STRIP_MODULE

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "can.h"
#include "afFieldProtocol.h"
#include "ledStrip.h"
#include "useful.h"


// Timer: (800Khz)
//     Ensure there is a clk source
//     PWM generation on channel
//     Prescalar: 0
//     Counter period: 119 (e.g. 96MHz/800KHz = 120)
//     Auto-reload preload enabled
//     PWM mode 2
//     DMA - TIM2_CH2, memory to peripheral, circular, word, word
//     Enable TIM2 global interrupt - priority medium


#define MAX_CURRENT_PER_RGB_LED_MA_x8 91 // 11.4mA for ws2812b eco
#define FIXED_CURRENT_PER_LED_MA_x8 11     // 1.32mA for ws2812b eco

#define LED_CLK_KHZ 800 // 1.25us per bit
#define CLOCK_RATE_KHZ 96000 // raw 48Mhz - divide by 2
#define COUNTER_PERIOD (CLOCK_RATE_KHZ/LED_CLK_KHZ)
#define ONE_HIGH_PERIOD   80 //((2 * COUNTER_PERIOD) / 3) // High for first 2/3 of period, High for ~416ns then low for ~833ns
#define ZERO_HIGH_PERIOD  40 //((1 * COUNTER_PERIOD) / 3) // High for first 1/3 of period, High for ~833ns then low for ~416ns
#define JITTER 32 // Out of 120 (so +/- 13%)

// Reset period for new panels is 50us but for old it's was longer
#define RESET_COUNT 10 // x 30us - Number of LED dma's - e.g. number of 24 bit cycles

// 24Mhz - period 41.66ns
// 1H_clocks - 33
// 1L_clocks - 19
// 0H_clocks - 17
// 0L_clocks - 35

#define ONE_LED_DMA_LENGTH 24 // 3 colours of 8 bits each
#define NUM_LEDS_PER_DMA (2*1)
#define FULL_DMA_BUFFER_LENGTH (NUM_LEDS_PER_DMA * ONE_LED_DMA_LENGTH)
#define MAX_LEDS (1000)
#define PIXEL_BUFFER_LENGTH (3 * MAX_LEDS) // Limited by the amount of ram we have 

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} colourStruct;

typedef struct {
	bool testing;
	TIM_HandleTypeDef * tim;
	TIM_HandleTypeDef * tim2;
	GPIO_TypeDef* testGpioX;
	uint16_t testGpioPin;
	uint32_t timChannel;
	// Config
	uint16_t numLedsAcross;
	uint16_t numLedsDown;
	uint8_t brightness;
	uint8_t userBrightness;
	uint16_t refreshPeriodMs;
	uint16_t maxCurrentMa;
	uint16_t spreadSpectrumPeriod;
	uint16_t lfsr;
	// Gamma correction
	uint8_t gammaCorrectionEnabled;
	uint8_t redAdjust;
	uint8_t greenAdjust;
	uint8_t blueAdjust;
	// This is a generic store for led colours 
	uint8_t pixelBuffer[PIXEL_BUFFER_LENGTH];
	// DMA state
	uint16_t dmaCount; // This count controls the current LED programming sequence
	// Main loop State
	uint32_t ticksSinceLastShow;
	uint16_t lastEstimatedCurrentMa;
	uint8_t exceededCurrent;
	bool show;
} ledStripDriver;

static ledStripDriver ledDrv = {};

__attribute__ ((section(".dmadata")))
uint32_t ledDmaBuffer[FULL_DMA_BUFFER_LENGTH];


static void showLedData();
void setShowField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
	showLedData();
}

static void setUserBrightness(uint8_t value) {
	uint16_t newBrightness = MIN(value, 100);
	ledDrv.userBrightness = newBrightness;
	ledDrv.brightness = (255 * newBrightness) / 100;
}

static void setSpeadSpectrumPeriod(uint16_t period) {
	if (period > 0) {
		__HAL_TIM_SET_AUTORELOAD(ledDrv.tim2, period);
	}
	if (period > 0 && ledDrv.spreadSpectrumPeriod == 0) {
    	softAssert(HAL_TIM_Base_Start_IT(ledDrv.tim2) == HAL_OK, "Failed to start timer");
	} else if (ledDrv.spreadSpectrumPeriod > 0 && period == 0) {
    	softAssert(HAL_TIM_Base_Stop_IT(ledDrv.tim2) == HAL_OK, "Failed to stop timer");
	}
	ledDrv.spreadSpectrumPeriod = period;
}

static void setUserBrightnessField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
	setUserBrightness(*data);
}
static void setSpeadSpectrumPeriodField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    uint16_t period;
    memcpy(&period, data, 2);
	setSpeadSpectrumPeriod(period);
}

#ifndef LED_STRIP_FIELDS_OFFSET
	#define LED_STRIP_FIELDS_OFFSET 0
#endif

static uint8_t one = 1;
uint8_t * const ledStripName = (uint8_t *) "display";
static sExtraMetaDataFields ledFieldMetaData              = {1, {{COMPONENT_NAME, ledStripName}}};
static sExtraMetaDataFields ledDaemonOnlyFieldMetaData    = {2, {{DAEMON_ONLY_VISIBLE, &one}, {COMPONENT_NAME, ledStripName}}};
static sExtraMetaDataFields brightnessCurrentMetaData     = {2, {{MAX_VALUE, &(uint8_t){100}}, {COMPONENT_NAME, ledStripName}}};
static sExtraMetaDataFields lastEstimatedCurrentMetaData  = {2, {{UNITS, (uint8_t *) "mA"}, {COMPONENT_NAME, ledStripName}}};
static sExtraMetaDataFields rerfreshPeriodMetaData        = {2, {{UNITS, (uint8_t *) "ms"}, {COMPONENT_NAME, ledStripName}}};
static sExtraMetaDataFields maxCurrentMetaData            = {3, {{SETTABLE, &(uint8_t){0}}, {UNITS, (uint8_t *) "mA"}, {COMPONENT_NAME, ledStripName}}};


sFieldInfoEntry ledStripFieldTable[] = {
	{ &ledDrv.userBrightness        , "brightness",               LED_STRIP_FIELDS_OFFSET + 0,               1,          AF_FIELD_TYPE_UINT,       1,   0,                      &brightnessCurrentMetaData, &setUserBrightnessField, NULL},
	{ &ledDrv.numLedsAcross         , "panel_width",              LED_STRIP_FIELDS_OFFSET + 1,               1,          AF_FIELD_TYPE_UINT,       2,   0,                      &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.numLedsDown           , "panel_height",             LED_STRIP_FIELDS_OFFSET + 2,               1,          AF_FIELD_TYPE_UINT,       2,   0,                      &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.refreshPeriodMs       , "refresh_period",           LED_STRIP_FIELDS_OFFSET + 3,               1,          AF_FIELD_TYPE_UINT,       2,   0,                      &rerfreshPeriodMetaData, NULL, NULL},
	{ &ledDrv.lastEstimatedCurrentMa, "estimated_current",        LED_STRIP_FIELDS_OFFSET + 4,               1,          AF_FIELD_TYPE_UINT,       2,   0,                      &lastEstimatedCurrentMetaData, NULL, NULL},
	{ &ledDrv.maxCurrentMa          , "max_current",              LED_STRIP_FIELDS_OFFSET + 5,               1,          AF_FIELD_TYPE_UINT,       2,   0,                      &maxCurrentMetaData, NULL, NULL},
	{ &ledDrv.gammaCorrectionEnabled, "gamma_correction_enabled", LED_STRIP_FIELDS_OFFSET + 6,               1,          AF_FIELD_TYPE_BOOLEAN,    1,   0,                      &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.redAdjust             , "gamma_correction_red",     LED_STRIP_FIELDS_OFFSET + 7,               1,          AF_FIELD_TYPE_UINT,       1,   0,                      &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.greenAdjust           , "gamma_correction_green",   LED_STRIP_FIELDS_OFFSET + 8,               1,          AF_FIELD_TYPE_UINT,       1,   0,                      &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.blueAdjust            , "gamma_correction_blue",    LED_STRIP_FIELDS_OFFSET + 9,               1,          AF_FIELD_TYPE_UINT,       1,   0,                      &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.pixelBuffer           , "led_colour",               LED_STRIP_FIELDS_OFFSET + 10,              MAX_LEDS-1, AF_FIELD_TYPE_UINT,       3,   DONT_REPLY_TO_SET_FLAG, &ledFieldMetaData, NULL, NULL},
	{ &ledDrv.spreadSpectrumPeriod  , "spread_spectrum_period",   LED_STRIP_FIELDS_OFFSET + 10 + MAX_LEDS-1, 1,          AF_FIELD_TYPE_UINT,       2,   0,                      &ledDaemonOnlyFieldMetaData, &setSpeadSpectrumPeriodField, NULL},
	{ NULL                          , "show",                     LED_STRIP_FIELDS_OFFSET + 10 + MAX_LEDS,   1,          AF_FIELD_TYPE_BOOLEAN,    1,   0,                      &ledFieldMetaData, &setShowField, NULL}
};
const uint32_t ledStripFieldTableSize = sizeof(ledStripFieldTable)/sizeof(sFieldInfoEntry);


// =========================================== //

static void fillDMABufferWithColourComponent(uint32_t * buffer, uint8_t colourComponent) {
	// Seems seem to take 120 cycles to execute (on STM32H7 at 192Mhz)
	buffer[0] = ZERO_HIGH_PERIOD;
	buffer[1] = ZERO_HIGH_PERIOD;
	buffer[2] = ZERO_HIGH_PERIOD;
	buffer[3] = ZERO_HIGH_PERIOD;
	buffer[4] = ZERO_HIGH_PERIOD;
	buffer[5] = ZERO_HIGH_PERIOD;
	buffer[6] = ZERO_HIGH_PERIOD;
	buffer[7] = ZERO_HIGH_PERIOD;
	if (colourComponent & 0x80)
		buffer[0] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x40)
		buffer[1] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x20)
		buffer[2] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x10)
		buffer[3] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x08)
		buffer[4] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x04)
		buffer[5] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x02)
		buffer[6] = ONE_HIGH_PERIOD;
	if (colourComponent & 0x01)
		buffer[7] = ONE_HIGH_PERIOD;
}

// ========================================== //

// sRGB - power = 2.4
// Without adding 
#define GCO 1 // Offset - add 1 as for low values the LED's are unlit which is confusing and often looks strange
// put into RAM for performance
__attribute__ ((section(".data")))
uint8_t gammaCorrectionTable[256] = {
	0, 0+GCO, 0+GCO, 0+GCO, 0+GCO, 0+GCO, 0+GCO, 1+GCO, 1+GCO, 1+GCO, 1+GCO, 1+GCO, 1+GCO, 1+GCO, 1+GCO, 1+GCO,
	1+GCO, 1+GCO, 2+GCO, 2+GCO, 2+GCO, 2+GCO, 2+GCO, 2+GCO, 2+GCO, 2+GCO, 3+GCO, 3+GCO, 3+GCO, 3+GCO, 3+GCO, 3+GCO,
	4+GCO, 4+GCO, 4+GCO, 4+GCO, 4+GCO, 5+GCO, 5+GCO, 5+GCO, 5+GCO, 6+GCO, 6+GCO, 6+GCO, 6+GCO, 7+GCO, 7+GCO, 7+GCO,
	8+GCO, 8+GCO, 8+GCO, 8+GCO, 9+GCO, 9+GCO, 9+GCO, 10+GCO, 10+GCO, 10+GCO, 11+GCO, 11+GCO, 12+GCO, 12+GCO, 12+GCO, 13+GCO,
	13+GCO, 13+GCO, 14+GCO, 14+GCO, 15+GCO, 15+GCO, 16+GCO, 16+GCO, 17+GCO, 17+GCO, 17+GCO, 18+GCO, 18+GCO, 19+GCO, 19+GCO, 20+GCO,
	20+GCO, 21+GCO, 22+GCO, 22+GCO, 23+GCO, 23+GCO, 24+GCO, 24+GCO, 25+GCO, 25+GCO, 26+GCO, 27+GCO, 27+GCO, 28+GCO, 29+GCO, 29+GCO,
	30+GCO, 30+GCO, 31+GCO, 32+GCO, 32+GCO, 33+GCO, 34+GCO, 35+GCO, 35+GCO, 36+GCO, 37+GCO, 37+GCO, 38+GCO, 39+GCO, 40+GCO, 41+GCO,
	41+GCO, 42+GCO, 43+GCO, 44+GCO, 45+GCO, 45+GCO, 46+GCO, 47+GCO, 48+GCO, 49+GCO, 50+GCO, 51+GCO, 51+GCO, 52+GCO, 53+GCO, 54+GCO,
	55+GCO, 56+GCO, 57+GCO, 58+GCO, 59+GCO, 60+GCO, 61+GCO, 62+GCO, 63+GCO, 64+GCO, 65+GCO, 66+GCO, 67+GCO, 68+GCO, 69+GCO, 70+GCO,
	71+GCO, 72+GCO, 73+GCO, 74+GCO, 76+GCO, 77+GCO, 78+GCO, 79+GCO, 80+GCO, 81+GCO, 82+GCO, 84+GCO, 85+GCO, 86+GCO, 87+GCO, 88+GCO,
	90+GCO, 91+GCO, 92+GCO, 93+GCO, 95+GCO, 96+GCO, 97+GCO, 99+GCO, 100+GCO, 101+GCO, 103+GCO, 104+GCO, 105+GCO, 107+GCO, 108+GCO, 109+GCO,
	111+GCO, 112+GCO, 114+GCO, 115+GCO, 116+GCO, 118+GCO, 119+GCO, 121+GCO, 122+GCO, 124+GCO, 125+GCO, 127+GCO, 128+GCO, 130+GCO, 131+GCO, 133+GCO,
	134+GCO, 136+GCO, 138+GCO, 139+GCO, 141+GCO, 142+GCO, 144+GCO, 146+GCO, 147+GCO, 149+GCO, 151+GCO, 152+GCO, 154+GCO, 156+GCO, 157+GCO, 159+GCO,
	161+GCO, 163+GCO, 164+GCO, 166+GCO, 168+GCO, 170+GCO, 171+GCO, 173+GCO, 175+GCO, 177+GCO, 179+GCO, 181+GCO, 183+GCO, 184+GCO, 186+GCO, 188+GCO,
	190+GCO, 192+GCO, 194+GCO, 196+GCO, 198+GCO, 200+GCO, 202+GCO, 204+GCO, 206+GCO, 208+GCO, 210+GCO, 212+GCO, 214+GCO, 216+GCO, 218+GCO, 220+GCO,
	222+GCO, 224+GCO, 226+GCO, 229+GCO, 231+GCO, 233+GCO, 235+GCO, 237+GCO, 239+GCO, 242+GCO, 244+GCO, 246+GCO, 248+GCO, 250+GCO, 253+GCO, 255,
};
//static uint8_t gammaCorrection(uint16_t colourComponent) {
//	// hardAssert(colourComponent < 256, "Values above 255 not supported");
//	return gammaCorrectionTable[colourComponent];
//}

//// TODO: I never got the gamma correction right
//static void fullGammaCorrect(colourStruct * colour) {
////	uint16_t desiredBrightness = (colour->red + colour->green + colour->blue);
//
//	colour->red   = (gammaCorrection(colour->red  ) * ledDrv.redAdjust)/256;
//	colour->green = (gammaCorrection(colour->green) * ledDrv.greenAdjust)/256;
//	colour->blue  = (gammaCorrection(colour->blue ) * ledDrv.blueAdjust)/256;
//
//	// This adds about 2.5us
//	// The gamma correction functions only handle up to 255 - so have to do something special when sum is greater than 255
//	// uint16_t actualBrightness;
//	// uint16_t sum = colour->red + colour->green + colour->blue;
//	// if (sum > 255) {
//	// 	actualBrightness = (350 * gammaUncorrection(sum / 2)) / 256;
//	// } else {
//	// 	actualBrightness = gammaUncorrection(sum);
//	// }
//	// uint16_t ratio = (256 * desiredBrightness) / actualBrightness;
//	// colour->red   = (ratio * colour->red  ) / 256;
//	// colour->green = (ratio * colour->green) / 256;
//	// colour->blue  = (ratio * colour->blue ) / 256;
//}

// ========================================== //


static void setBuffer(uint16_t index, colourStruct colour) {
	hardAssert(index < MAX_LEDS, "Index out of range");
	ledDrv.pixelBuffer[index * 3 + 0] = colour.blue;
	ledDrv.pixelBuffer[index * 3 + 1] = colour.green;
	ledDrv.pixelBuffer[index * 3 + 2] = colour.red;
}

// ========================================== //
// Fill DMA buffers with the correct pixel from the pixelBuffer

// Each LED takes 30us to program - so we need to calculate each LED in much less than 30us
// Currently takes 11us (without gamma correction
// Effectively 13us including interrupt time
static void setupNextLedDma(uint32_t * buffer) {
	static uint32_t current = 0;
	static uint32_t maxCurrent = 0;
	uint32_t numLeds = ledDrv.numLedsAcross * ledDrv.numLedsDown;
	uint32_t ledIndex = ledDrv.dmaCount - RESET_COUNT;
	
	if (ledDrv.dmaCount < RESET_COUNT) {
		if (ledDrv.dmaCount == 0) {
			current = (FIXED_CURRENT_PER_LED_MA_x8 * numLeds * 256) / MAX_CURRENT_PER_RGB_LED_MA_x8;
			maxCurrent = (ledDrv.maxCurrentMa * 256 * 8) / MAX_CURRENT_PER_RGB_LED_MA_x8;
		}
		memset(buffer, 0, ONE_LED_DMA_LENGTH * sizeof(ledDmaBuffer[0]));

	} else if (ledIndex < numLeds) {
		// Find the pixel crossponding to the led
		// The LED panels have every other rows flipped (because it snakes)
		uint32_t pixelIndex = ledIndex;
		uint32_t ledDown = ledIndex / ledDrv.numLedsAcross;
		uint32_t ledAcross = ledIndex % ledDrv.numLedsAcross;
		bool rowFlipped = (ledDown % 2 == 1);
		if (rowFlipped) {
			pixelIndex = (ledIndex + ledDrv.numLedsAcross - 1 - (2 * ledAcross));
		}

		// Do brightness and gamma correction here because the operation is lossy - this way the raw colours remain intact			
		// Could use a separate buffer to store gamma corrected, brightness corrected values?
		uint32_t brightness = ledDrv.brightness;
		uint8_t * pixelBuffer = &ledDrv.pixelBuffer[pixelIndex * 3];
		uint32_t blue  = (pixelBuffer[0] * brightness)/256;
		uint32_t green = (pixelBuffer[1] * brightness)/256;
		uint32_t red   = (pixelBuffer[2] * brightness)/256;

		if (ledDrv.gammaCorrectionEnabled) {
			red   = (gammaCorrectionTable[red  ] * ledDrv.redAdjust  )/256;
			green = (gammaCorrectionTable[green] * ledDrv.greenAdjust)/256;
			blue  = (gammaCorrectionTable[blue ] * ledDrv.blueAdjust )/256;
		}
		
		fillDMABufferWithColourComponent(&buffer[0], green);
		fillDMABufferWithColourComponent(&buffer[8], red);
		fillDMABufferWithColourComponent(&buffer[16], blue);

		// Update the expected current
		current += (red + green + blue);
		// Dim the LED strip if max current exceeded 
		if (current > maxCurrent) {
			ledDrv.exceededCurrent = 1;
			setUserBrightness(5);
			ledDrv.dmaCount = 0; // Go back to the beginning and set use the lower brightness
		}
	} else {
		ledDrv.lastEstimatedCurrentMa = (MAX_CURRENT_PER_RGB_LED_MA_x8 * current ) / (256 * 8);
		memset(buffer, 0, ONE_LED_DMA_LENGTH * sizeof(ledDmaBuffer[0]));
		if (ledDrv.dmaCount >= (numLeds + RESET_COUNT + RESET_COUNT)) {
			ledDrv.show = false;
		}
	}
	ledDrv.dmaCount++;
}

void ledStrip_TIM_PWM_PulseFinishedCallback() {
	if (!ledDrv.show)
		return;
	if (ledDrv.testing)
		return;
	// Finished using the second half of the buffer so we can now overwrite it with the next lot of data
	setupNextLedDma(&ledDmaBuffer[FULL_DMA_BUFFER_LENGTH/2]);
}
void ledStrip_TIM_PWM_PulseFinishedHalfCpltCallback() {
	if (!ledDrv.show)
		return;
	if (ledDrv.testing)
		return;
	// Finished using the first half of the buffer so we can now overwrite it with the next lot of data
	setupNextLedDma(&ledDmaBuffer[0]);
}

static void showLedData() {
	// This will start the DMA reset loop
	ledDrv.dmaCount = 0;
	ledDrv.show = true;
	ledDrv.ticksSinceLastShow = HAL_GetTick();
}

static void initialiseLedDma() {
	// The DMA will run continuously - we never stop it we just control it's output
	HAL_StatusTypeDef status = HAL_TIM_PWM_Start_DMA(ledDrv.tim, ledDrv.timChannel, (uint32_t *) &ledDmaBuffer[0], FULL_DMA_BUFFER_LENGTH);
	softAssert(status == HAL_OK, "LedStrip DMA start failed with error");
}

void ledStripSpreadSpectrumTimerCallback() {
	uint16_t lfsr = ledDrv.lfsr;
	// 7,9,13 triplet from wikipedia originally from
	// http://www.retroprogramming.com/2017/07/xorshift-pseudorandom-numbers-in-z80.html
	lfsr ^= lfsr >> 7;
	lfsr ^= lfsr << 9;
	lfsr ^= lfsr >> 13;
	uint32_t period = ((lfsr * JITTER) / 65536) - (JITTER/2) + COUNTER_PERIOD;
	// ledDrv.tim->Init.Period = period;
	__HAL_TIM_SET_AUTORELOAD(ledDrv.tim, period);
	ledDrv.lfsr = lfsr;
}


uint8_t selfTestLedStrip() {
	uint8_t failed = 0;
	ledDrv.testing = 1; // stop DMA buffer being set by the callback
	// Write dma all 0's and check output
	HAL_Delay(1);
	memset(ledDmaBuffer, 0, FULL_DMA_BUFFER_LENGTH * sizeof(ledDmaBuffer[0]));
	HAL_Delay(1);
	GPIO_PinState state = HAL_GPIO_ReadPin(ledDrv.testGpioX, ledDrv.testGpioPin);
	failed |= (state != GPIO_PIN_RESET);
	softAssert(!failed, "LED Self test failed");

	// Write dma all 1's and check output
	memset(ledDmaBuffer, 0xFF, FULL_DMA_BUFFER_LENGTH * sizeof(ledDmaBuffer[0]));
	HAL_Delay(1);
	state = HAL_GPIO_ReadPin(ledDrv.testGpioX, ledDrv.testGpioPin);
	failed |= (state != GPIO_PIN_SET);
	softAssert(!failed, "LED Self test failed");

	ledDrv.testing = 0;
	return failed;
}

void initialiseLedStrip(TIM_HandleTypeDef * tim, TIM_HandleTypeDef * tim2, uint32_t timChannel, GPIO_TypeDef* testGpioX, uint16_t testGpioPin) {
	memset(ledDmaBuffer, 0, sizeof(ledDmaBuffer));
	addComponentFieldTableToGlobalTable(ledStripFieldTable, ledStripFieldTableSize);
	memset(&ledDrv, 0, sizeof(ledDrv));
	ledDrv.lfsr = 0xACE1u;
	ledDrv.testGpioX = testGpioX;
	ledDrv.testGpioPin = testGpioPin;
	ledDrv.tim = tim;
	ledDrv.tim2 = tim2;
	ledDrv.timChannel = timChannel;
	ledDrv.maxCurrentMa = 2000;
	ledDrv.gammaCorrectionEnabled = 1;
	ledDrv.redAdjust = 255;
	ledDrv.greenAdjust = 200;
	ledDrv.blueAdjust = 220;
	ledDrv.refreshPeriodMs = 50;
	// Set initial LED pattern just to signal it's working
	ledDrv.numLedsAcross = 8;
	ledDrv.numLedsDown = 24;
	
	// setSpeadSpectrumPeriod(120 * 8); // jitter every 8 bits
	
	setUserBrightness(50);

	// Start the DMA
	initialiseLedDma();
	// selfTestLedStrip();

	// Takes 0.2 seconds
	for (uint16_t i=0; i<192; i++) {
		colourStruct cyanGreen = {0x0, 0xFF, 0x80};
		setBuffer(i, cyanGreen);
		if (i % 6 == 0)
			showLedData();
		// Turn leds on one by one
		HAL_Delay(1);
	}
	showLedData();
}

void afterCanSetupLedStrip() {
	for (uint16_t i=0; i<200; i++) {
		colourStruct magenta = {0xff, 0x00, 0xff};
		setBuffer(i, magenta);
	}
	showLedData();
}

void loopLedStrip(uint32_t ticks) {
	// Refresh
	if ((ticks - ledDrv.ticksSinceLastShow) > ledDrv.refreshPeriodMs && ledDrv.refreshPeriodMs != 0) {
		showLedData();
	}
	
	// Every 0.5s pulish the estimated current 
	static uint32_t lastHalfsecondTime = 0;
	if ((ticks - lastHalfsecondTime) > 500) {
		lastHalfsecondTime = ticks;
		publishFieldsIfBelowBandwidth(LED_STRIP_FIELDS_OFFSET + 4, LED_STRIP_FIELDS_OFFSET + 4);
	}
	// Every 5 seconds publish the rest of the fields
	static uint32_t last5secondTime = 0;
	if ((ticks - last5secondTime) > 5000) {
		last5secondTime = ticks;
		publishFieldsIfBelowBandwidth(LED_STRIP_FIELDS_OFFSET, LED_STRIP_FIELDS_OFFSET + 11);
	}
	
	// If max current exceeded publish the brightness to show it's been set to 0
	if (ledDrv.exceededCurrent) {
		ledDrv.exceededCurrent = 0;
		publishFieldsIfBelowBandwidth(0, 0);
	}
}

#endif // ENABLE_LED_STRIP
