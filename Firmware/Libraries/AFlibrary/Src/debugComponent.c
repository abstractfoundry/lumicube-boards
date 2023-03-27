/* Copyright (c) 2021 Abstract Foundry Limited */

#include "project.h"
#include <stdint.h>
#include <string.h>
#include "afException.h"
#include "useful.h"
#include "afFieldProtocol.h"

#define DEBUG_FIELD_OFFSET 60000 // Just needs to be sufficiently high not to conflict with other fields

uint32_t rdpLevel = 0;

uint32_t readRdpValue() {
    FLASH_OBProgramInitTypeDef pOBInit;
    HAL_FLASHEx_OBGetConfig(&pOBInit);
    rdpLevel = pOBInit.RDPLevel;
    return rdpLevel;
}

void setRdp(uint8_t value) {
    #ifdef F0
        uint8_t rdpRegVal;
    #endif
	#ifdef F7
		uint8_t rdpRegVal;
	#endif
    #ifdef H7
        uint32_t rdpRegVal;
    #endif
    if (value == 0) {
        rdpRegVal = OB_RDP_LEVEL_0;
    } else if (value == 1) {
        rdpRegVal = OB_RDP_LEVEL_1;
    } else if (value == 2) {
#ifndef PRODUCTION
    return;
#endif
        rdpRegVal = OB_RDP_LEVEL_2;
    } else {
        hardAssert(0, "Invalid RDP");
        return;
    }
    FLASH_OBProgramInitTypeDef pOBInit;
    HAL_FLASHEx_OBGetConfig(&pOBInit);
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    pOBInit.OptionType = OPTIONBYTE_RDP;
    pOBInit.RDPLevel = rdpRegVal;
    HAL_FLASHEx_OBProgram(&pOBInit);
    HAL_FLASH_OB_Launch();
}

static void setRdpField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    setRdp(*data);
}

static uint8_t one = 1;
static sExtraMetaDataFields debugFieldMetaData = {1, {{DAEMON_ONLY_VISIBLE, &one}}};
// NOTE: rdp_level is set using 0,1,2 but it is read back as the actual 32 bit register value
sFieldInfoEntry debugFieldTable[] = {
    { &rdpLevel,           "rdp_level",      DEBUG_FIELD_OFFSET+0,                   1,  AF_FIELD_TYPE_UINT,      4, 0, &debugFieldMetaData, &setRdpField, NULL },
    { &assertCount,        "assert_count",   DEBUG_FIELD_OFFSET+1,                   1,  AF_FIELD_TYPE_UINT,      2, 0, &debugFieldMetaData, NULL, NULL },
    { &assertBuffer[0],    "asserts",        DEBUG_FIELD_OFFSET+2,  ASSERT_BUFFER_SIZE,  AF_FIELD_TYPE_UTF8_CHAR, 1, 0, &debugFieldMetaData, NULL, NULL },
};
const uint32_t debugFieldTableSize = sizeof(debugFieldTable)/sizeof(sFieldInfoEntry);

void initialiseDebug() {
    addComponentFieldTableToGlobalTable(debugFieldTable, debugFieldTableSize);
    readRdpValue();
}

void loopDebug(uint32_t ticks) {
    static uint32_t next = 0;
    if (ticks > next) {
        next = ticks + 5000;
        readRdpValue();
        broadcastFields(DEBUG_FIELD_OFFSET, DEBUG_FIELD_OFFSET + 1);
    }
}


