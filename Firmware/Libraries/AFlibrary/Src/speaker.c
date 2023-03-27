/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_SPEAKER_MODULE

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "can.h"
#include "useful.h"
#include "afFieldProtocol.h"
#include "speaker.h"


// DAC:
//     Project manager -> advanced settings -> Register callback: DAC enable
//     Enable OUT1 and OUT2 (only external pin)
//     Disable output buffer for both, trigger: timer trigger out event 
//     DMA:
//         Add DAC1
//         Memory to peripheral
//         Circular
//         Data width: word, word
//     NVIC DMA priority: medium 
// Timer (for 8KHz):
//     Prescalar: 0
//     Counter period: 5999 (to give 16KHz - but the firmware will continually change this anyway)
//     Trigger event selection TRGO: update event
// GPIO:
//    Set the shutdown pin to be GPIO output
// ADC:
//    Enable ADC for testing

#define MAX_DAC_VALUE 4095
#define SPEAKER_DMA_BUFFER_BYTES (64)
#define NUM_SPEAKER_DATA_FIELDS 112
#define SPEAKER_BUFFER_BYTES (4*1024) // We don't use most of this - we use 1KB at 16KHz, 

// #define MAX_SECONDS_RECORDED 256
// #define NUM_HISTORY 300

typedef struct {
    TIM_HandleTypeDef * tim;
    DAC_HandleTypeDef * dac;
    ADC_HandleTypeDef *testAdc;
    GPIO_TypeDef* shutdownGPIOx;
    uint16_t shutdownGPIOPin;
    sCircularBuffer dmaBuffer;
    int16_t bufferData[SPEAKER_BUFFER_BYTES/2];
    sCircularBuffer buffer;
    uint16_t userVolume;
    uint16_t volume;
    uint32_t dataFreeSpaceInBytes;
    uint32_t lastDmaValue;
    uint32_t frequency;
    uint32_t autoReloadPeriod;
    // Clock correction
    bool disableClockCorrection;
    uint32_t nominalReloadPeriod;
    uint32_t veryLowWaterMark;
    uint32_t lowWaterMark;
    uint32_t highWaterMark;
    uint32_t veryHighWaterMark;
    uint32_t veryLowDiff;
    uint32_t lowDiff;
    int32_t highDiff;
    int32_t veryHighDiff;

    uint32_t clippingLow;
    uint32_t clippingHigh;
    uint32_t totalOverflows;
    uint32_t totalUnderruns;
    // uint32_t overflowsPerSecond[MAX_SECONDS_RECORDED];
    // uint32_t underrunsPerSecond[MAX_SECONDS_RECORDED];
} sSpeakerDriver;

static sSpeakerDriver speaker = {};

__attribute__ ((section(".dmadata")))
uint32_t speakerDmaBuffer[SPEAKER_DMA_BUFFER_BYTES/4];

// uint32_t autoReloadHistoryIndex = 0;
// __attribute__ ((section(".dmadata")))
// uint16_t autoReloadHistory[NUM_HISTORY] = {};
// __attribute__ ((section(".dmadata")))
// uint16_t lengthHistory[NUM_HISTORY] = {};

static void setSpeakerData(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    // Somewhat ignore the fields - just add any new data to the buffer
    field_t numEntries = endFieldIndex - startFieldIndex;
    if(appendBuffer(&speaker.buffer, data, numEntries*2) < 0) {
        speaker.totalOverflows++;
        // softAssert(0, "Too much data to speaker");
    }
}
static void setUserVolume(uint16_t volume) {
    uint16_t newUserVolume = MIN(500, volume); // Can have up to 5x volume
    speaker.userVolume = newUserVolume;
    speaker.volume = (newUserVolume * 256) / 100;
}
static void setFrequency(uint32_t freq) {
    speaker.frequency = freq;
    speaker.autoReloadPeriod = (SPEAKER_TIMER_CLK_FREQUENCY/speaker.frequency) - 1;
    speaker.nominalReloadPeriod = speaker.autoReloadPeriod;
    uint32_t bufferSize = freq * 2000 / 16000; // Needs about 2000 at 16KHz (62.5ms of latency)
    bufferSize = MAX(bufferSize, 1000); // Use at least 1000 bytes
    bufferSize = MIN(bufferSize, SPEAKER_BUFFER_BYTES);
    speaker.veryLowWaterMark  = (bufferSize * 10) / 100;
    speaker.lowWaterMark      = (bufferSize * 25) / 100;
    speaker.highWaterMark     = (bufferSize * 75) / 100;
    speaker.veryHighWaterMark = (bufferSize * 90) / 100;
    speaker.veryLowDiff       = ( 12 * 16000 / freq);
    speaker.lowDiff           = (  4 * 16000 / freq);
    speaker.highDiff          = (( -4 * 16000) / (int32_t) freq);
    speaker.veryHighDiff      = ((-12 * 16000) / (int32_t) freq);
}

static void setUserVolumeField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    uint16_t volume;
    memcpy(&volume, data, 2);
    setUserVolume(volume);
}
static void setFrequencyField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    uint32_t freq = 0;
    memcpy(&freq, data, sizeof(freq));
    setFrequency(freq);
}

#ifndef SPEAKER_FIELDS_OFFSET
    #define SPEAKER_FIELDS_OFFSET 0
#endif

static uint8_t one = 1;
uint8_t * const speakerName = (uint8_t *) "speaker";
static sExtraMetaDataFields speakerFieldMetaData = {1, {{COMPONENT_NAME, speakerName}}};
static sExtraMetaDataFields daemonOnlySpeakerMetaData    = {2, {{DAEMON_ONLY_VISIBLE, &one}, {COMPONENT_NAME, speakerName}}};
//static sExtraMetaDataFields speakerNonSettableMetaData = {}; //{2, {{SETTABLE, &zero}, {COMPONENT_NAME, speakerName}}};

sFieldInfoEntry speakerFieldTable[] = {
    { NULL,                          "data",                 SPEAKER_FIELDS_OFFSET,                                NUM_SPEAKER_DATA_FIELDS,  AF_FIELD_TYPE_INT,      2, DONT_REPLY_TO_SET_FLAG, &speakerFieldMetaData,      &setSpeakerData,     NULL },
    { &speaker.userVolume,           "volume",               SPEAKER_FIELDS_OFFSET + NUM_SPEAKER_DATA_FIELDS,      1,                        AF_FIELD_TYPE_UINT,     2, 0,                      &speakerFieldMetaData,      &setUserVolumeField, NULL },
    { &speaker.frequency,            "frequency",            SPEAKER_FIELDS_OFFSET + NUM_SPEAKER_DATA_FIELDS + 1,  1,                        AF_FIELD_TYPE_UINT,     4, 0,                      &daemonOnlySpeakerMetaData, &setFrequencyField,  NULL },
    { &speaker.autoReloadPeriod ,    "autoReloadPeriod",     SPEAKER_FIELDS_OFFSET + NUM_SPEAKER_DATA_FIELDS + 2,  1,                        AF_FIELD_TYPE_UINT,     4, 0,                      &daemonOnlySpeakerMetaData, NULL, NULL },
    { &speaker.dataFreeSpaceInBytes, "free_space_in_bytes",  SPEAKER_FIELDS_OFFSET + NUM_SPEAKER_DATA_FIELDS + 3,  1,                        AF_FIELD_TYPE_UINT,     4, 0,                      &daemonOnlySpeakerMetaData, NULL, NULL },
};
const uint32_t speakerFieldTableSize = sizeof(speakerFieldTable)/sizeof(sFieldInfoEntry);

int32_t movingAverageLengthMultiplied = 0;
void writeHalfDma(uint32_t * dmaPtr) {
    uint32_t length = bufferLength(&speaker.buffer);

    if (!speaker.disableClockCorrection) {
        uint32_t newReloadPeriod;
        // Add some hysteris - if it's set to very low only come out of it when you reach high
        // likewise if set to very high, only come out when you reach low
        if (length < speaker.veryLowWaterMark) {
            newReloadPeriod = speaker.nominalReloadPeriod + speaker.veryLowDiff; // Rapidly slow down
        } else if ((length < speaker.lowWaterMark)
            && (speaker.autoReloadPeriod != (speaker.nominalReloadPeriod + speaker.veryLowDiff))) {
            newReloadPeriod = speaker.nominalReloadPeriod + speaker.lowDiff; // Slow down
        } else if (length > speaker.veryHighWaterMark) {
            newReloadPeriod = speaker.nominalReloadPeriod + speaker.veryHighDiff; // Rapidly speed up
        } else if ((length > speaker.highWaterMark)
            && (speaker.autoReloadPeriod != (speaker.nominalReloadPeriod + speaker.veryHighDiff))) {
            newReloadPeriod = speaker.nominalReloadPeriod + speaker.highDiff; // speed up
        } else {
            // Medium level - leave as is
            newReloadPeriod = speaker.autoReloadPeriod;
        }
        if (newReloadPeriod != speaker.autoReloadPeriod) {
            speaker.autoReloadPeriod = newReloadPeriod;
            __HAL_TIM_SetAutoreload(speaker.tim, speaker.autoReloadPeriod);
        }
    }
    
    // autoReloadHistory[autoReloadHistoryIndex] =  speaker.autoReloadPeriod;
    // lengthHistory[autoReloadHistoryIndex] = length;
    // autoReloadHistoryIndex++;
    // autoReloadHistoryIndex = autoReloadHistoryIndex % NUM_HISTORY;

    if (length > SPEAKER_DMA_BUFFER_BYTES) { // 2x as much as it needs just as extra buffer
        int16_t data[SPEAKER_DMA_BUFFER_BYTES/8];
        popBuffer(&speaker.buffer, (uint8_t *) data, SPEAKER_DMA_BUFFER_BYTES/4);

        for (uint16_t i=0; i<SPEAKER_DMA_BUFFER_BYTES/8; i++) {
            int32_t value = (speaker.volume * data[i]) / 256;
            value = ((value / 16) + MAX_DAC_VALUE/2);
            if (value < 0) {
                speaker.clippingLow++;
                value = 0;
            }
            if (value > MAX_DAC_VALUE) {
                speaker.clippingHigh++;
                value = MAX_DAC_VALUE;
            }
            dmaPtr[i] = value; // Convert the integer to DAC value
        }
        speaker.lastDmaValue = dmaPtr[(SPEAKER_DMA_BUFFER_BYTES/8) - 1];
    } else {
        for (uint16_t i=0; i<SPEAKER_DMA_BUFFER_BYTES/8; i++) {
            // volatile static uint64_t sbindex = 0;
            // uint32_t freq = 500;
            // uint16_t tmpIndex = ((sbindex * freq * 1024) / TIM_FREQ) % 1024;
            // int16_t data2 = fastSin16bit(tmpIndex) / 20;
            // sbindex++;
            // speaker.lastDmaValue = ((data2 / 16) + 2048);
            dmaPtr[i] = speaker.lastDmaValue;
        }
        speaker.totalUnderruns++;
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef * hdac) {
    writeHalfDma(&speakerDmaBuffer[SPEAKER_DMA_BUFFER_BYTES/8]);
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef * hdac) {
    writeHalfDma(&speakerDmaBuffer[0]);
}

void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef * hdac) {
    softAssert(0, "DAC error callback")
}

extern ADC_HandleTypeDef hadc3; // Speaker test

// 4ms for 1000 ADC conversions => 4us per conversion =>
// uint32_t vals[1024] = {};
uint8_t selfTestSpeaker() {
//    volatile uint32_t average = 0;
    volatile uint32_t min = -1;
    volatile uint32_t max = 0;
    // Just based on observation the average is around 24000
    uint32_t total = 0;
    softAssert(HAL_ADC_Start(speaker.testAdc) == HAL_OK, "");
    // HAL_Delay(10);
    for (uint32_t i=0; i<1024; i++) {
        HAL_ADC_Start(speaker.testAdc);
        softAssert(HAL_ADC_PollForConversion(speaker.testAdc, 2) == HAL_OK, "");
        uint32_t val = HAL_ADC_GetValue(speaker.testAdc);
        total += val;
        min = MIN(min, val);
        max = MAX(max, val);
        // vals[i] = val;
    }
    HAL_ADC_Stop(speaker.testAdc);
//    average = total / 1024;
    uint8_t failed = 0; //(average < 5000 || average > 20000);
    failed |= (max < 60000) || (min > 5000);
	softAssert(!failed, "Speaker Self test failed");
    return failed;
}

void initialiseSpeaker(TIM_HandleTypeDef * tim, DAC_HandleTypeDef * dac, GPIO_TypeDef * shutdownGPIOx, uint16_t shutdownGPIOPin, ADC_HandleTypeDef * testAdc) {
    memset(speakerDmaBuffer, 0, sizeof(speakerDmaBuffer));
    addComponentFieldTableToGlobalTable(speakerFieldTable, speakerFieldTableSize);
    setUserVolume(50);
    speaker.tim = tim;
    speaker.dac = dac;
    speaker.testAdc = testAdc;
    speaker.shutdownGPIOx = shutdownGPIOx;
    speaker.shutdownGPIOPin = shutdownGPIOPin;
    speaker.buffer.data = (uint8_t *) speaker.bufferData;
    speaker.buffer.size = SPEAKER_BUFFER_BYTES;
    speaker.dmaBuffer.size = SPEAKER_DMA_BUFFER_BYTES;
    speaker.dmaBuffer.data = (uint8_t *) speakerDmaBuffer;
    speaker.lastDmaValue = MAX_DAC_VALUE/2;
    // setFrequency(16000);
    setFrequency(32000);
    HAL_GPIO_WritePin(speaker.shutdownGPIOx, speaker.shutdownGPIOPin, GPIO_PIN_SET); // Enable Amp    
    // We just set DAC 2 to always be 0 - if we wanted extra gain we could set it to the inverse of the DAC1 value
    softAssert(HAL_OK == HAL_DAC_Start(speaker.dac, DAC_CHANNEL_1), "");
    softAssert(HAL_OK == HAL_DAC_Start(speaker.dac, DAC_CHANNEL_2), "");
    softAssert(HAL_DAC_SetValue(speaker.dac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, MAX_DAC_VALUE/2) == HAL_OK, "setting DAC 1 failed");
    softAssert(HAL_DAC_SetValue(speaker.dac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, MAX_DAC_VALUE/2) == HAL_OK, "setting DAC 2 failed");
    // Start the DAC1 DMA and the timer that causes it to update
    softAssert(HAL_TIM_Base_Start_IT(speaker.tim) == HAL_OK, "Failed to start speaker timer");
    softAssert(HAL_OK == HAL_DAC_Start_DMA(speaker.dac, DAC_CHANNEL_1, (uint32_t *) speakerDmaBuffer, SPEAKER_DMA_BUFFER_BYTES/4, DAC_ALIGN_12B_R), "");

    // Wait 50ms for the speaker output to be stable
    HAL_Delay(50);
    selfTestSpeaker();
}

void afterCanSetupSpeaker() {
}

void loopSpeaker(uint32_t ticks) {
    static uint32_t nextTicks1 = 0;
    if (ticks > nextTicks1) {
        nextTicks1 = ticks + 200;
        speaker.dataFreeSpaceInBytes = speaker.buffer.size - bufferLength(&speaker.buffer);
        publishFieldsIfBelowBandwidth(SPEAKER_FIELDS_OFFSET + NUM_SPEAKER_DATA_FIELDS + 1, SPEAKER_FIELDS_OFFSET + NUM_SPEAKER_DATA_FIELDS + 3);
    }

    // // Keep track of the latest underruns
    // static uint32_t nextTicks = 0;
    // static uint32_t lastUnderruns = 0;
    // static uint32_t lastOverflows = 0;
    // static uint32_t index = 0;
    // if (ticks > nextTicks) {
    //     nextTicks = ticks + 1000;
    //     speaker.underrunsPerSecond[index] = speaker.totalUnderruns - lastUnderruns;
    //     speaker.overflowsPerSecond[index] = speaker.totalOverflows - lastOverflows;
    //     lastUnderruns = speaker.totalUnderruns;
    //     lastOverflows = speaker.totalOverflows;
    //     index = (index+1) % MAX_SECONDS_RECORDED;
    // }

    // // Every 5 seconds make a pulse
    // static uint32_t nextTicks = 0;
    // if (ticks > nextTicks) {
    //     nextTicks = ticks + 5000;

    //     // Test with a 500 Hz wave
    //     // Takes 1ms
    //     uint32_t freq = 500;
    //     uint16_t data[64*10*2]; // 0.08 seconds of data
    //     for (uint32_t i =0; i<64*10; i++) {
    //         data[i] = fastSin16bit((i * freq * 1024) / speaker.frequency);
    //     }
    //     appendBuffer(&speaker.buffer, (uint8_t *) &data, 64*10*2);
    // }
}

#endif //ENABLE_SPEAKER_MODULE

