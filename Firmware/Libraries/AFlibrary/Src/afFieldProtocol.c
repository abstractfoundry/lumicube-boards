/* Copyright (c) 2020 Abstract Foundry Limited */

#include "project.h"
#include <string.h>
#include "useful.h"
#include "afException.h"
#include "can.h"
#include "afFieldProtocol.h"

#define MAX_PACKET_BYTES 256
#define MAX_COMPONENTS_PER_PROJECT 10

// First byte of the command contains 4 bits of the discriminator, and 4 bits indicating the number of parameter bytes

// Discriminators
#define AF_COMMAND_DISCRIMINATOR_SENDING_N_FIELDS (0 << 4)
#define AF_COMMAND_DISCRIMINATOR_SKIP_FIELDS      (1 << 4)
#define AF_COMMAND_DISCRIMINATOR_END_DICTIONARY   (2 << 4)


const uint8_t metaFieldSize[] = META_DATA_FIELD_SIZES;

sExtraMetaDataFields nonSettableMetaData = {1, {{SETTABLE, &(uint8_t){0}}}};


// ============================= //
// Functions to handle the field info table

uint8_t numComponents = 0;
sFieldInfoEntry * componentTables[MAX_COMPONENTS_PER_PROJECT] = {};
uint32_t componentTableSizes[MAX_COMPONENTS_PER_PROJECT] = {};

void addComponentFieldTableToGlobalTable(sFieldInfoEntry * table, uint32_t tableSize) {
    // Ignore if called more than once
    for (uint8_t i=0; i<numComponents; i++) {
        if (componentTables[i] == table)
            return;
    }
    if (numComponents >= MAX_COMPONENTS_PER_PROJECT) {
        softAssert(0, "Exceeded MAX_COMPONENTS_PER_PROJECT");
        return;
    }

    // Just check no fields overlap
    for (field_t k=0; k<tableSize; k++) {
        sFieldInfoEntry * newField = &table[k];
        for (uint8_t i=0; i<numComponents; i++) {
            for (field_t j=0; j<componentTableSizes[i]; j++) {
                sFieldInfoEntry * otherField = &componentTables[i][j];
                if (newField->startFieldIndex < otherField->startFieldIndex) {
                    // check field end is before other start
                    softAssert(newField->startFieldIndex + newField->span <= otherField->startFieldIndex, "Fields indicies overlap");
                } else {
                    // check other field end is before field start
                    softAssert(otherField->startFieldIndex + otherField->span <= newField->startFieldIndex, "Fields indicies overlap");
                }
            }
        }
    }

    // Could just put entry at the end but for minor performance improvement for 
    // getFieldInfo place entries in order of increasing field indicies

    // Find pos to insert entry
    uint8_t i = 0;
    for (; i<numComponents; i++) {
        if (table[0].startFieldIndex < componentTables[i][0].startFieldIndex) {
            break;
        }
    }
    // Shift rest of table up
    for (uint8_t j=numComponents; j>i; j--) {
        componentTables[j] = componentTables[j-1];
        componentTableSizes[j] = componentTableSizes[j-1];
    }
    // Insert new entry
    componentTables[i] = table;
    componentTableSizes[i] = tableSize;
    numComponents++;
}

const sFieldInfoEntry * getFieldInfo(field_t fieldIndex) {
    // Find table
    sFieldInfoEntry * table = componentTables[numComponents-1];
    uint32_t tableSize = componentTableSizes[numComponents-1];
    for (uint8_t i=0; i<numComponents; i++) {
        if (fieldIndex < componentTables[i][0].startFieldIndex) {
            if (i == 0) {
                return NULL;
            } else {
                table = componentTables[i-1];
                tableSize = componentTableSizes[i-1];
                break;
            }
        }
    }
    softAssert(table, "");

    // Find field

    // Binary search
   uint32_t first = 0;
   uint32_t last = tableSize - 1;
   while (first <= last) {
       uint32_t middle = (first+last)/2;
        const sFieldInfoEntry * middleEntry = &table[middle];
        if (fieldIndex < (middleEntry->startFieldIndex)) {
           last = middle - 1;
        } else if (fieldIndex < middleEntry->startFieldIndex + middleEntry->span) {
            return middleEntry;
        } else {
           first = middle + 1;
        }
    }

    // This might actually be quicker
    // for (uint32_t f=0; f<tableSize; f++) {
    // 	if (fieldIndex < table[f].startFieldIndex) {
    // 		// If before first entry
    // 		if (f == 0)
    // 			return NULL;
    // 		const sFieldInfoEntry * entry = &table[f-1];
    // 		// If not in field span
    // 		if (fieldIndex > entry->span + entry->startFieldIndex || fieldIndex < entry->startFieldIndex) {
    // 			return NULL;
    // 		}
    // 		return entry;
    // 	}
    // }
    return NULL;
}

// Only used for metadata so doesn't need to be quick
const sFieldInfoEntry * getNextValidMetaDataFieldInfo(field_t fieldIndex, field_t metaDataFieldIndex, field_t * validFieldIndex, field_t * validMetaDataFieldIndex) {
    // First find the table
    // Just loop through all tables and fields
    for (uint8_t t=0; t<numComponents; t++) {
        for (uint32_t f=0; f<componentTableSizes[t]; f++) {
            sFieldInfoEntry * field = &componentTables[t][f];
            if (fieldIndex < (field->startFieldIndex + field->span)) {
                // Found the correct entry

                // Check valid meta data field index - else move on to the next field entry
                sExtraMetaDataFields * extra = field->extraMetaData;
                field_t maxMetaDataField = (extra && extra->numFields > 0) ? extra->fields[extra->numFields-1].index : (NUM_FIELD_INFO_FIELDS-1);

                // If field index is before this entry then use this entry
                if (fieldIndex < field->startFieldIndex) {
                    *validFieldIndex = field->startFieldIndex;
                    *validMetaDataFieldIndex = 0;
                    return field;

                // If meta data is greater than max move on to the next field entry
                } else if (metaDataFieldIndex > maxMetaDataField) {
                    // Continue

                } else {
                    // Valid field
                    *validFieldIndex = fieldIndex;
                    *validMetaDataFieldIndex = metaDataFieldIndex;
                    return field;
                }
            }
        }
    }
    // Reached end
    return NULL;
}


// ========================================= //
// Field packet receiving

static void readCommand(uint8_t * data, uint8_t dataLength, uint8_t * bytePos, uint8_t * discriminator, uint32_t * parameter) {
    *discriminator = data[*bytePos] & 0xF0;
    uint8_t parameterLength = data[*bytePos] & 0x0F;
    (*bytePos)++;
    uint8_t * ptr = data + *bytePos;
    switch (parameterLength) {
        case 0:
            *parameter = 0;
            break;
        case 1:
            *parameter = ptr[0];
            break;
        case 2:
            *parameter = ptr[0] | ((ptr[1] << 8) & 0xFF00);
            break;
        case 4:
            *parameter = ptr[0] | ((ptr[1] << 8) & 0xFF00) | ((ptr[2] << 16) & 0xFF0000) | ((ptr[3] << 24) & 0xFF000000);
            break;
        default:
            hardAssert(0, "Unsupported parameter length");
    }
    (*bytePos) += parameterLength;
}

void afProtocolProcessRx(uint8_t * data, uint8_t dataLength) {
    uint8_t bytePos = 0;
    field_t fieldIndex = 0;
    static const sFieldInfoEntry * fieldInfo = NULL;

    while (bytePos < dataLength) {
        uint32_t parameter;
        uint8_t discriminator;
        readCommand(data, dataLength, &bytePos, &discriminator, &parameter);

        switch (discriminator) {
            case AF_COMMAND_DISCRIMINATOR_SKIP_FIELDS:
                fieldIndex += parameter;
                break;

            case AF_COMMAND_DISCRIMINATOR_SENDING_N_FIELDS: {
//                softAssert(parameter == 1, "");
                field_t endFieldIndex = fieldIndex + parameter;
                while (fieldIndex < endFieldIndex) {

                    // Get base field info
                    if ((fieldIndex < fieldInfo->startFieldIndex || fieldIndex >= fieldInfo->startFieldIndex + fieldInfo->span) || (fieldInfo == NULL)) {
                        fieldInfo = getFieldInfo(fieldIndex);
                        hardAssert(fieldInfo, "Invalid field index");
                        hardAssert(fieldInfo->size > 0, "setFields - Variable field length not supported");
                    }
                    // Work out how many fields we can set at once
                    field_t fieldOffset = fieldIndex - fieldInfo->startFieldIndex;
                    field_t numFields = MIN((endFieldIndex - fieldIndex), (fieldInfo->span - fieldOffset));
                    uint32_t numBytes = fieldInfo->size * numFields;
                    hardAssert(bytePos + numBytes <= dataLength, "Set fields overflowed!");

                    // Do the set
                    if (fieldInfo->setFieldFn) {
                        fieldInfo->setFieldFn(fieldInfo, fieldIndex, fieldIndex+numFields, &data[bytePos]);
                    } else {
                        void * fieldPtr = (((uint8_t *) fieldInfo->field) + (fieldInfo->size * fieldOffset));
                        memcpy(fieldPtr, &data[bytePos], numBytes);
                    }
                    // Reply
                    if (!DONT_REPLY_TO_SET(fieldInfo->flags)) {
                        broadcastFields(fieldIndex, fieldIndex + numFields - 1);
                    }

                    // Increment counters
                    bytePos += numBytes;
                    fieldIndex += numFields;
                }
                break;
            }

            default:
                hardAssert(0, "Invalid discriminator");
        }
    }
}

// ========================================= //
// Field packet creation

typedef struct {
    uint8_t * data;
    uint16_t size;
    uint16_t maxSize;
} sFieldPacket;

static int8_t addBytesToPacket(sFieldPacket * packet, uint8_t * bytes, uint16_t numBytes) {
    if(packet->size + numBytes >= packet->maxSize)
        return -1;
    memcpy(&packet->data[packet->size], bytes, numBytes);
    packet->size += numBytes;
    return 0;
}

static int8_t addCommandToPacket(sFieldPacket * packet, uint8_t discriminator, uint32_t parameter) {
    if(packet->size + 5 > packet->maxSize)
        return -1;
    uint8_t * data = &packet->data[packet->size];
    if (parameter  <= 0xFF) {
        data[0] = discriminator | 1;
        *(data + 1) = parameter & 0xFF;
        packet->size += 2;
    } else if (parameter  <= 0xFFFF) {
        data[0] = discriminator | 2;
        *(data + 1) = parameter & 0xFF;
        *(data + 2) = (parameter >> 8) & 0xFF;
        packet->size += 3;
    } else {
        data[0] = discriminator | 4;
        *(data + 1) = parameter & 0xFF;
        *(data + 2) = (parameter >> 8) & 0xFF;
        *(data + 3) = (parameter >> 16) & 0xFF;
        *(data + 4) = (parameter >> 24) & 0xFF;
        packet->size += 5;
    }
    return 0;
}

static int8_t addVariableLengthFieldDataToPacket(sFieldPacket * packet, uint8_t * fieldData, uint8_t fieldDataSize) {
    // Always need header byte that contains the length 
    // NOTE: This means the max length is 255
    if (addBytesToPacket(packet, &fieldDataSize, 1) < 0)
        return -1;
    if (addBytesToPacket(packet, fieldData, fieldDataSize) < 0)
        return -1;
    return 0;
}

static int8_t addSkipToPacket(sFieldPacket * packet, uint32_t numToSkip) {
    return addCommandToPacket(packet, AF_COMMAND_DISCRIMINATOR_SKIP_FIELDS, numToSkip);
}

static int8_t addSendingFieldsToPacket(sFieldPacket * packet, uint32_t numFields) {
    return addCommandToPacket(packet, AF_COMMAND_DISCRIMINATOR_SENDING_N_FIELDS, numFields);
}

static void addMultiFieldDataToPacket(sFieldPacket * packet, field_t * nextPacketFieldIndex, field_t startFieldIndex,
                                        field_t endFieldIndex, uint8_t * data, uint32_t numDataBytes) {
    // Skip if field is not consecutive
    if (startFieldIndex > (*nextPacketFieldIndex))
        addSkipToPacket(packet, startFieldIndex - *nextPacketFieldIndex );
    *nextPacketFieldIndex = endFieldIndex;
    // Sending N fields
    addSendingFieldsToPacket(packet, endFieldIndex + 1 - startFieldIndex);

    // Field data
    for (field_t fieldIndex=startFieldIndex; fieldIndex<=endFieldIndex;) {
        const sFieldInfoEntry * fieldInfo = getFieldInfo(fieldIndex);
        hardAssert(fieldInfo != NULL, "Inv field index");
        field_t fieldOffset = fieldIndex - fieldInfo->startFieldIndex;
        field_t tmpEndFieldIndex = MIN((fieldInfo->startFieldIndex + fieldInfo->span - 1), endFieldIndex);

        if (numDataBytes > 0) {
            softAssert(addBytesToPacket(packet, data, numDataBytes) == 0, "Not enough memory for packet");

        } else if (!fieldInfo->getFieldFn) {
            field_t numFields = tmpEndFieldIndex - fieldInfo->startFieldIndex + 1;
            uint32_t numBytes = numFields * fieldInfo->size;
            if (fieldInfo->field == NULL) {
                // Add zeroes - TODO: this should all be done more efficiently
                for (uint16_t i=0; i<numBytes; i++) {
                    uint8_t tmp = 0;
                    softAssert(addBytesToPacket(packet, &tmp, numBytes) == 0, "Not enough memory for packet");
                }
            } else {
                void * fieldPtr = (void *) (((uint8_t *) fieldInfo->field) + (fieldInfo->size * fieldOffset));
                softAssert(addBytesToPacket(packet, fieldPtr, numBytes) == 0, "Not enough memory for packet");
            }
        } else {
            uint8_t value[8];
            fieldInfo->getFieldFn(fieldInfo, fieldIndex, fieldIndex, value);
            softAssert(addBytesToPacket(packet, value, fieldInfo->size) == 0, "Not enough memory for packet");
        }

        fieldIndex = tmpEndFieldIndex + 1;
    }
}

// General buffer for creating packets
uint8_t globalPacketData[MAX_PACKET_BYTES] = {};
sFieldPacket globalPacket = {globalPacketData, 0, MAX_PACKET_BYTES};

static sFieldPacket * createFieldPacket(field_t startFieldIndex, field_t endFieldIndex) {
    globalPacket.size = 0;
    field_t nextPacketFieldIndex = 0;
    addMultiFieldDataToPacket(&globalPacket, &nextPacketFieldIndex, startFieldIndex, endFieldIndex, NULL, 0);
    return &globalPacket;
}

void setFields(uint8_t destNodeId, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data, uint32_t numBytes) {
    globalPacket.size = 0;
    field_t nextPacketFieldIndex = 0;
    addMultiFieldDataToPacket(&globalPacket, &nextPacketFieldIndex, startFieldIndex, endFieldIndex, data, numBytes);
    requestSetFields(destNodeId, globalPacket.data, globalPacket.size);
}

void broadcastFields(field_t startFieldIndex, field_t endFieldIndex) {
    sFieldPacket * packet = createFieldPacket(startFieldIndex, endFieldIndex);
    broadcastFieldsPacket(packet->data, packet->size);
}

int8_t publishFieldsIfBelowBandwidth(field_t startFieldIndex, field_t endFieldIndex) {
    sFieldPacket * packet = createFieldPacket(startFieldIndex, endFieldIndex);
    return publishFieldPacketIfBelowBandwidth(packet->data, packet->size);
}

// ========================================= //
// Meta data

// The incoming data for a meta data request specifies a position.
// The board will then send meta data from this position onwards
// The position is specified by a field index and a sub field index.
// A sending N fields means move on to the next level
void parseFrameForMetaDataFieldIndex(uint8_t * data, uint8_t dataLength, field_t * fieldIndex, field_t * subFieldIndex) {
    uint8_t discriminator = 0; // Don't need to be initialised but avoid compiler warnings
    uint32_t parameter = 0; // Don't need to be initialised but avoid compiler warnings
    uint8_t numFieldIndicies = 0;
    
    uint8_t bytePos=0;
    while ( bytePos < dataLength) {
        readCommand(data, dataLength, &bytePos, &discriminator, &parameter);

        if (discriminator == AF_COMMAND_DISCRIMINATOR_SKIP_FIELDS) {
            if (numFieldIndicies == 0) {
                (*fieldIndex) += parameter;
            } else if (numFieldIndicies == 1) {
                (*subFieldIndex) += parameter;
            }

        } else if (discriminator == AF_COMMAND_DISCRIMINATOR_SENDING_N_FIELDS) {
            hardAssert(parameter == 1, "");
            hardAssert(numFieldIndicies < 2, "More levels of meta data requested than expected");
            numFieldIndicies++;

        } else {
            hardAssert(0, "Unsupported discriminator");
        }
    }
}

static uint8_t addToMetaDataFieldToPacket(sFieldPacket * packet, eMetaDataFieldIndex * packetFieldIndex, eMetaDataFieldIndex fieldIndex, uint8_t * value) {
    int8_t res = 0;
    if (fieldIndex > *packetFieldIndex)
        res |= addSkipToPacket(packet, fieldIndex - *packetFieldIndex);
    addSendingFieldsToPacket(packet, 1);
    if (metaFieldSize[fieldIndex] == 0) {
        res |= addVariableLengthFieldDataToPacket(packet, value, strlen((char *) value));
    } else {
        res |= addBytesToPacket(packet, value, metaFieldSize[fieldIndex]);
    }
    *packetFieldIndex = fieldIndex + 1;
    return (res != 0); // Return true if full
}

void createMetaDataPacket(sFieldPacket * packet, field_t startFieldIndex, field_t startMetaDataFieldIndex) {
    packet->size = 0;
    field_t fieldIndex;
    field_t metaDataFieldIndex;
    const sFieldInfoEntry * fieldInfo = getNextValidMetaDataFieldInfo(startFieldIndex, startMetaDataFieldIndex, &fieldIndex, &metaDataFieldIndex);
    if (fieldInfo == NULL) {
        return;
    }
    
    // if (fieldIndex >= 60000) {
    //     softAssert(0, "");
    // }

    // Skip to the field and start sending meta data fields
    if (fieldIndex > 0)
        addSkipToPacket(packet, fieldIndex);
    addSendingFieldsToPacket(packet, 1);
    
    uint8_t packetFull = 0;
    eMetaDataFieldIndex packetMetaFieldIndex = 0;
    if (NAME >= metaDataFieldIndex) {
        addSendingFieldsToPacket(packet, 1);
        addVariableLengthFieldDataToPacket(packet, (uint8_t *) fieldInfo->name, strlen(fieldInfo->name));
        packetMetaFieldIndex = 1;
    }
    if (TYPE >= metaDataFieldIndex)
        packetFull = addToMetaDataFieldToPacket(packet, &packetMetaFieldIndex, TYPE, (uint8_t *) &fieldInfo->type);
    if ((SIZE >= metaDataFieldIndex) && !packetFull)
        packetFull = addToMetaDataFieldToPacket(packet, &packetMetaFieldIndex, SIZE, (uint8_t *) &fieldInfo->size);
    if ((SPAN >= metaDataFieldIndex) && !packetFull)
        packetFull = addToMetaDataFieldToPacket(packet, &packetMetaFieldIndex, SPAN, (uint8_t *) &fieldInfo->span);
    sExtraMetaDataFields * extraMetaData = fieldInfo->extraMetaData;
    if (extraMetaData) {
        for (eMetaDataFieldIndex i=0; i<extraMetaData->numFields; i++) {
            if (extraMetaData->fields[i].index < packetMetaFieldIndex) {
                softAssert(0, "Meta data fields need to be in order");
                return;
            } else if ((extraMetaData->fields[i].index >= metaDataFieldIndex) && !packetFull) {
                packetFull = addToMetaDataFieldToPacket(packet, &packetMetaFieldIndex, extraMetaData->fields[i].index, extraMetaData->fields[i].value);
            }
        }
    }
    if (packetFull && packet->size == 0)
        softAssert(0, "Failed to fit any data in packet");
}

void handleMetaDataRequest(uint8_t * data, uint8_t dataLength, uint8_t ** responsePacket, uint16_t * responseLength) {
    field_t startFieldIndex = 0;
    field_t startMetaDataFieldIndex = 0;
    parseFrameForMetaDataFieldIndex(data, dataLength, &startFieldIndex, &startMetaDataFieldIndex);
//    softAssert(startFieldIndex != 122, "");
	globalPacket.size = 0;
	createMetaDataPacket(&globalPacket, startFieldIndex, startMetaDataFieldIndex);
    *responsePacket = globalPacket.data;
    *responseLength = globalPacket.size;
}

