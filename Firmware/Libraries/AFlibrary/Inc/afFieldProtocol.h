/* Copyright (c) 2020 Abstract Foundry Limited */

#ifndef APP_INC_AFFIELDPROTOCOL_H_
#define APP_INC_AFFIELDPROTOCOL_H_

// #include "project.h"

#include <stdint.h>

#define field_t uint32_t

#define AF_FIELD_TYPE_NULL 0
#define AF_FIELD_TYPE_RAW 1
#define AF_FIELD_TYPE_ENUM 2
#define AF_FIELD_TYPE_BOOLEAN 3
#define AF_FIELD_TYPE_UINT 4
#define AF_FIELD_TYPE_INT 5
#define AF_FIELD_TYPE_FLOAT 6
#define AF_FIELD_TYPE_TIME 7
#define AF_FIELD_TYPE_UTF8_CHAR 8
#define AF_FIELD_TYPE_UTF8_STRING 9
#define AF_FIELD_TYPE_DICTIONARY 10

#define AF_FIELD_SIZE_VARIABLE_SIZE 0

typedef enum {
	NAME = 0, // string, size: variable
	TYPE = 1, // enum, size: 1 bytes
	SIZE = 2, // uint, size: 1 bytes
	SPAN = 3, // uint, size: 4 bytes
	GETTABLE = 4, // boolean
	SETTABLE = 5, // boolean
	IDEMPOTENT = 6, // type: boolean
	MIN_VALUE = 7, // uint, variable size
	MAX_VALUE = 8, // uint, variable size
	UNITS = 9, // type: string
	DEBUG_TYPE = 10, // boolean
	DAEMON_ONLY_VISIBLE = 11, // boolean (SYSTEM_FIELD / INTERNAL_FIELD)
	COMPONENT_NAME = 12, // type: string
} eMetaDataFieldIndex;

#define META_DATA_FIELD_SIZES  { \
	0, /* NAME */ \
	1, /* TYPE */ \
	1, /* SIZE */ \
	4, /* SPAN - Array with this many consecutive fields */ \
	1, /* GETTABLE */ \
	1, /* SETTABLE */ \
	1, /* IDEMPOTENT */ \
	0, /* MIN_VALUE */ \
	0, /* MAX_VALUE */ \
	0, /* UNITS */ \
	1, /* DEBUG_TYPE */ \
	1, /* DAEMON_ONLY_VISIBLE */ \
	0, /* COMPONENT_NAME */ \
}


typedef struct {
	eMetaDataFieldIndex index;
	uint8_t * value;
} sMetaDataField;

typedef struct {
	uint8_t numFields;
	sMetaDataField fields[];
} sExtraMetaDataFields;

typedef struct sFieldInfoEntry sFieldInfoEntry;

// endFieldIndex is not inclusive
typedef void (setFieldsFnType)(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data);
typedef void (getFieldsFnType)(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data);

struct sFieldInfoEntry {
	void * field;
	char * name;
	field_t startFieldIndex;
	field_t span;
	uint8_t type;
	uint8_t size;
	uint8_t flags;
	sExtraMetaDataFields * extraMetaData;
	setFieldsFnType * setFieldFn;
	getFieldsFnType * getFieldFn;
}; // 19 bytes (rounded to 20) per row (plus the size of the field name string (which is often around 15))



// #define CALL_SET_METHOD_FLAG 0x1
// #define CALL_GET_METHOD_FLAG 0x2
#define DONT_REPLY_TO_SET_FLAG 0x4

// #define CALL_SET_METHOD(val) (val & CALL_SET_METHOD_FLAG)
// #define CALL_GET_METHOD(val) (val & CALL_GET_METHOD_FLAG)
#define DONT_REPLY_TO_SET(val) (val & DONT_REPLY_TO_SET_FLAG)

#define NUM_FIELD_INFO_FIELDS 4

extern sExtraMetaDataFields nonSettableMetaData;

void addComponentFieldTableToGlobalTable(sFieldInfoEntry * componentTable, uint32_t tableSize);
void handleMetaDataRequest(uint8_t * data, uint8_t dataLength, uint8_t ** responsePacket, uint16_t * responseLength);
void broadcastFields(field_t startFieldIndex, field_t endFieldIndex);
int8_t publishFieldsIfBelowBandwidth(field_t startFieldIndex, field_t endFieldIndex);
void setFields(uint8_t destNodeId, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data, uint32_t numBytes);


// void afProtocolProcessRx(CanardRxTransfer *transfer, readAfNextDataFunc * readDataFn);

#endif /*APP_INC_AFFIELDPROTOCOL_H_*/

