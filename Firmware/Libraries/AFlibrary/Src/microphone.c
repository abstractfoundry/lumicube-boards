/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_MICROPHONE_MODULE

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "can.h"
#include "useful.h"
#include "afFieldProtocol.h"
#include "microphone.h"
#include "pdm2pcm.h"

// I2S: Half duplex master
//    Master Mode receive
//    I2S philips
//    16 bits on 16 bit frame
//    32 Khz
//    RX DMA:
//        Circular, Half word
//
// PDM2PCM:
//     BIT_ORDER_LSB
//     ENDIANNESS_BE
//     in_ptr 1
//     out_ptr 1
//     decimation factor 64
//     Output samples 16
//     Mic gain 10

extern PDM_Filter_Handler_t PDM1_filter_handler;

#define DECIMATION_FACTOR (64)
#define PDM_BUFFER_SAMPLES ((DECIMATION_FACTOR*2) * 1)
#define PCM_BUFFER_BYTES (1024*16)
#define NUM_MICROPHONE_DATA_FIELDS 112

typedef struct {
    I2S_HandleTypeDef * i2s;
    PDM_Filter_Handler_t * PDM1_filter_handler;
    bool enable;
    uint16_t maxSoundLevel;
    uint8_t pcmData[PCM_BUFFER_BYTES]; // Processed data
    sCircularBuffer pcmBuffer;
    uint16_t data[NUM_MICROPHONE_DATA_FIELDS]; // Data being sent out
} sMicrophoneDriver;

static sMicrophoneDriver microphone = {};

__attribute__ ((section(".dmadata")))
uint16_t microphonePdmData[PDM_BUFFER_SAMPLES]; // Raw data from microphone


void setDmaEnable(bool enable) {
    if (!microphone.enable && enable) {
        HAL_I2S_Receive_DMA(microphone.i2s, &microphonePdmData[0], PDM_BUFFER_SAMPLES);
    } else if(microphone.enable && !enable) {
        HAL_I2S_DMAStop(microphone.i2s);
    }
    microphone.enable = enable;
}

void setDmaEnableField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    setDmaEnable(*data);
}

#ifndef MICROPHONE_FIELDS_OFFSET
    #define MICROPHONE_FIELDS_OFFSET 0
#endif

static uint8_t zero = 0;
static uint8_t * const microphoneName = (uint8_t *) "microphone";
static sExtraMetaDataFields microphoneFieldMetaData        = {1, {{COMPONENT_NAME, microphoneName}}};
static sExtraMetaDataFields microphoneNonSettableMetaData  = {2, {{SETTABLE, &zero}, {COMPONENT_NAME, microphoneName}}};

sFieldInfoEntry microphoneFieldTable[] = {
    { &microphone.data,     "data",    MICROPHONE_FIELDS_OFFSET,     NUM_MICROPHONE_DATA_FIELDS,  AF_FIELD_TYPE_INT,     2,   0,  &microphoneNonSettableMetaData,  NULL,  NULL},
    { &microphone.enable,   "enable",  MICROPHONE_FIELDS_OFFSET+NUM_MICROPHONE_DATA_FIELDS,   1,  AF_FIELD_TYPE_BOOLEAN, 1,   0,  &microphoneFieldMetaData,  &setDmaEnableField,  NULL},
    // { &microphone.enable,   "max_sound_level",  MICROPHONE_FIELDS_OFFSET+NUM_MICROPHONE_DATA_FIELDS+1,   1,  AF_FIELD_TYPE_UINT,    2,   0,  &microphoneFieldMetaData,  &setDmaEnableField,  NULL},
};
const uint32_t microphoneFieldTableSize = sizeof(microphoneFieldTable)/sizeof(sFieldInfoEntry);


static void filter(uint16_t * pdm) {
    #define PCM_SAMPLES (((PDM_BUFFER_SAMPLES / 2) * 16)/DECIMATION_FACTOR)
    uint16_t tmpPcmData[PCM_SAMPLES];
    for (uint32_t i=0; i<(PCM_SAMPLES/16); i++) {
        // Take DECIMATION_FACTOR samples and decimate it to 16 samples
        uint32_t result = PDM_Filter(&pdm[i * DECIMATION_FACTOR], &tmpPcmData[i*16], &PDM1_filter_handler);
        softAssert(result == 0, "Filter failed");

    }
    appendBuffer(&microphone.pcmBuffer, (uint8_t *) tmpPcmData, PCM_SAMPLES*2);
//    softAssert(appendBuffer(&microphone.pcmBuffer, (uint8_t *) tmpPcmData, PCM_SAMPLES*2) == 0, "Buffer overflowed");
}

void microphone_SPI_RxHalfCpltCallback () {
    filter(&microphonePdmData[0]);
}
void microphone_SPI_RxCpltCallback () {
    filter(&microphonePdmData[PDM_BUFFER_SAMPLES/2]);
}

uint8_t selfTestMicrophone() {
    // Microphone should be up and running so check we have some data
    HAL_Delay(1);
    volatile uint16_t minVal = 0xFFFF;
    volatile uint16_t maxVal = 0;
    for (uint32_t i=0; i<NUM_MICROPHONE_DATA_FIELDS; i++) {
        maxVal = MAX(maxVal, microphonePdmData[i]);
        minVal = MIN(minVal, microphonePdmData[i]);
    }
    // Test that there are values coming in
    uint8_t failed = ((maxVal - minVal) < 1000);
    softAssert(!failed, "Microphone self test failed");
    return failed;
}

// #include "usbd_def.h"
// #include "usbd_cdc_if.h"
// extern USBD_HandleTypeDef hUsbDeviceFS;
// static bool usbTransmissionInProgress() {
//     USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*) &hUsbDeviceFS.pClassData;
//     if (hcdc->TxState != 0){
//         return 1;
//     }
//     return 0;
// }
//    while (1) {
//        #define NUM_TO_SEND (1024)
//        if ((bufferLength(&microphone.pcmBuffer) > NUM_TO_SEND) && !usbTransmissionInProgress()) {
//            int16_t tmp[NUM_TO_SEND];
//            uint32_t result = popBuffer(&microphone.pcmBuffer, tmp, NUM_TO_SEND);
//            softAssert(result == 0, "Buffer overflowed");
//            CDC_Transmit_FS(tmp, NUM_TO_SEND);
//        }
//    }

void initialiseMicrophone(I2S_HandleTypeDef * i2s) {
    memset(microphonePdmData, 0, sizeof(microphonePdmData));
    addComponentFieldTableToGlobalTable(microphoneFieldTable, microphoneFieldTableSize);
    microphone.i2s = i2s;
    microphone.pcmBuffer.data = (uint8_t *) microphone.pcmData;
    microphone.pcmBuffer.size = PCM_BUFFER_BYTES;
    microphone.PDM1_filter_handler = &PDM1_filter_handler;
    setDmaEnable(1);
    selfTestMicrophone();
}

void afterCanSetupMicrophone() {
}

void loopMicrophone(uint32_t ticks) {
    while (bufferLength(&microphone.pcmBuffer) > (2*NUM_MICROPHONE_DATA_FIELDS)) {
        popBuffer(&microphone.pcmBuffer, (uint8_t *) microphone.data, 2*NUM_MICROPHONE_DATA_FIELDS);
        publishFieldsIfBelowBandwidth(0, NUM_MICROPHONE_DATA_FIELDS-1);
    }
    
    static uint32_t nextTicks1 = 0;
    if (ticks > nextTicks1) {
        nextTicks1 = ticks + 5000;
        publishFieldsIfBelowBandwidth(MICROPHONE_FIELDS_OFFSET+NUM_MICROPHONE_DATA_FIELDS, MICROPHONE_FIELDS_OFFSET+NUM_MICROPHONE_DATA_FIELDS);
    }
}

#endif //ENABLE_MICROPHONE_MODULE

