/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_SERIAL_PROTOCOL_MODULE

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "useful.h"
#include "afFieldProtocol.h"
#include "canard.h"
#include "can.h"
#include "serialProtocol.h"


extern uint8_t getLocalNodeId();
// Respond to serial link CAN frames once they've been processed
// int16_t respondToUavCanFrame(CanardRxTransfer *requestTransfer, uint8_t dataTypeId, const void* payload, uint16_t payloadLength);



// Implementation details
#define SP_PROTOCOL_VERSION 1
#define SP_NUM_WINDOW_PACKETS 16
#define SP_MAX_PACKETS_IN_CHANNEL 255 // Maybe make smaller?

#define SP_MAX_SEQUENCE_NUMBER 256

// TODO: sort out how big all the buffer need to be, window size and max tx packets in outgoing buffer
// Buffer sizes
#define SP_MAX_TX_RESPONSES 16
#define SP_MAX_TX_REQUESTS 64

#define WRAP_WAIT_TIME_MS 8 // Probably only needs to be around 3?

//========================//

#define COBS_DELIMITER_BYTE 0x00

// Requests
#define SP_PING 0x00
#define SP_INITIALISE 0x1E // Arguments: ProtocolVersion, InitialSequenceNumber
#define SP_UAVCAN 0x2D // Arguments: SequenceNumber CANID0, CANID1, CANID2, CANID3, TransferID, Payload0, Payload1, ...
// #define SP_RAW_CAN 0x33
// RESERVED_C4 0x4B
// RESERVED_C5 0x55
// RESERVED_C6 0x66
// RESERVED_C7 0x78
// RESERVED_C8 0x87
// RESERVED_C9 0x99

// Responses
#define SP_ACKNOWLEDGE 0xAA // Arguments: LastAcceptedSequenceNumber
#define SP_INITIALISED 0xB4
#define SP_UNINITIALISED 0xCC
#define SP_PONG 0xFF // Arguments: HighestSupportedProtocolVersion
// RESERVED_RD 0xD2
// RESERVED_RE 0xE1

#define IS_SP_RESPONSE(CMD) (CMD & 0x80)
#define IS_SP_REQUEST(CMD) (!(CMD & 0x80))


typedef uint16_t tPacketIndex;
typedef uint16_t tPacketLength;

// TODO: maybe reset these if the link goes away
typedef struct {
    uint32_t pings;
    uint32_t pongs;
    uint32_t initialises;
    uint32_t uninitialised;
    uint32_t initialised;
    uint32_t uavCanFrames;
    uint32_t uavCanFramesAcked;
} spLinkStats;

typedef struct {
    spLinkStats egress;
    spLinkStats ingress;
    // Errors
    uint32_t corruptedFrameData;
    uint32_t rxCrcErrors;
    uint32_t rxCobsDecodingErrors;
    uint32_t rxInvalidPacketLength; // Too big or too small
    uint32_t rxInvalidCommand;
    uint32_t txRequestsMetaDataFull;
    uint32_t txRequestDataFull;
    uint32_t uartFailedToTransmit;
    uint32_t canToSerialBufferOverflowed;
    // Other
    uint32_t txWindowWrap;
    uint32_t notNextSequenceNum;
} spStats;

typedef struct {
    bool pong;
    bool ack;
    bool uninitialised;
    bool initialised;
} spResponses;

// Requests are made up of two buffers:
// Data buffer which just has the packet data
// Info buffer which has the meta data about where the packets are
typedef struct {
    uint16_t head;
    tPacketLength length;
    uint32_t timeSent; // Try and wait 4ms before wrapping and retransmitting - the pi loop is called every 2.5ms
} spTxRequestMetaData;

typedef struct {
    uint32_t startSequenceNumber;
    uint32_t endSequenceNumber;
    spTxRequestMetaData entries[SP_MAX_TX_REQUESTS];
    uint32_t start;
    uint32_t end;
    uint32_t next;
    sCircularBuffer * rawbuffer; // contains the packet data to go out on the wire
} spTxRequests;

typedef struct {
    // State between calls
    uint16_t cycleSinceLastInit;
    uint32_t pongsReceived;
    bool initialised;
    spResponses responses;
    spTxRequests requests;
    uint8_t * smallPacketPtr; // Length = SP_MAX_INIT_REQUEST_PACKET_SIZE + SP_MAX_RESPONSE_PACKET_SIZE
} spTxState;

typedef struct {
    bool initialised;
    tPacketIndex nextExpectedSequenceNumber;
    tPacketIndex lastAckedSequenceNumber;
    sCircularBuffer * buffer;
    uint8_t packet[SP_MAX_PACKET_SIZE];
    uint32_t tmpPacketLength;
    uint8_t * tmpPacketData;
    bool discarding;
    bool specialWrapMode;
    uint32_t nextZero;
    uint32_t crc;
} spRxState;

typedef struct spInterface {
    spTxState tx;
    spRxState rx;
    spStats stats;
    tTransmitFn * transmitFn;
    tRxTailFn * rxTailFn;
} spInterface; // ~1KB

uint32_t wrapWaitTimeMs = WRAP_WAIT_TIME_MS;

#define SP_REQUEST_OVERHEAD (2 + CRC_COBS_OVERHEAD)


/*==================================*/
// UAVCAN packet packing/unpacking

void packBroadcastUavCanHeader(uint8_t * headerBytes, uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority) {
    headerBytes[0] = 0x7F & sourceNodeId;
    headerBytes[1] = 0xFF & dataTypeId;
    headerBytes[2] = 0xFF & (dataTypeId >> 8);
    headerBytes[3] = priority;

    // Anonomous frames - the daemon doesn't actually need the proper discriminator as it's used for reassembling CAN frames into UAVCAN frames
    if (sourceNodeId == 0) {
        // const uint16_t discriminator = 0;
        headerBytes[1] = (dataTypeId & 0x3);
        headerBytes[2] = 0;
    }
}

void packRequestResponseUavCanHeader(uint8_t * headerBytes, uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId, uint8_t priority) {
    headerBytes[0] = 0x80 | (0x7F & sourceNodeId);
    headerBytes[1] = (request ? 0x80 : 0x0) | (0x7F & destNodeId);
    headerBytes[2] = dataTypeId;
    headerBytes[3] = priority;
}

void unpackUavCanHeader(uint8_t * headerBytes, bool * isBroadcast, uint8_t * sourceNodeId, uint8_t * destNodeId, bool * request, uint16_t * dataTypeId, uint8_t * priority) {
    *isBroadcast = !(0x80 & headerBytes[0]);
    *sourceNodeId = 0x7F & headerBytes[0];
    *priority = headerBytes[3];
    if (*isBroadcast) {
        *dataTypeId = (headerBytes[2] << 8) | headerBytes[1];
    } else {
        *request = 0x80 & headerBytes[1];
        *destNodeId = 0x7F & headerBytes[1];
        *dataTypeId = headerBytes[2];
    }
}


/*==================================*/
// COBS and CRC

// 512 bytes - put into RAM for performance
__attribute__ ((section(".data")))
uint16_t LUT[] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};
// static void crcUpdate(uint16_t * currentCrc, uint8_t byte) {
//     *currentCrc = ((*currentCrc << 8) & 0xFFFF) ^ LUT[(*currentCrc >> 8) ^ byte];
// }
// static void crcInit(uint16_t * currentCrc) {
//     *currentCrc = 0xFFFF;
// }

// TODO: for acks create custom crcCobs encoding as it's only a 6byte frame
static void crcAndCobsEncodeData(uint8_t * packetWithDataAtPositionOne, uint32_t packetLength) {
    uint8_t * lastDelimiter = &packetWithDataAtPositionOne[0];
    uint8_t * data = &packetWithDataAtPositionOne[1];
    uint32_t bytesLeft = packetLength - CRC_COBS_OVERHEAD;
    uint8_t * last3Bytes = &packetWithDataAtPositionOne[packetLength - 3];
    uint32_t count = 1;
    uint32_t crc = 0xFFFF;
    // Modify the data in place and calculate the CRC and COBS encode
    uint8_t * byte = data;
    for (; bytesLeft--; byte++, count++) {
        crc = ((crc << 8) & 0xFFFF) ^ LUT[(crc >> 8) ^ *byte];
        if (*byte == COBS_DELIMITER_BYTE) {
            *lastDelimiter = count;
            lastDelimiter = byte;
            count = 0;
        }
    }
    // Now fill in the CRC and COBS encode it as well
    last3Bytes[0] = (crc >> 8) & 0xFF;
    last3Bytes[1] = crc & 0xFF;
    for (uint32_t i=0; i<2; i++, count++) {
        if (last3Bytes[i] == COBS_DELIMITER_BYTE) {
            *lastDelimiter = count;
            lastDelimiter = &last3Bytes[i];
            count = 0;
        }
    }
    *lastDelimiter = count;
    last3Bytes[2] = COBS_DELIMITER_BYTE;
}

// static int8_t crcAndCobsDecode(spInterface * sp, uint8_t * packet, uint32_t packetLength) {
//     softAssert(packet[0] > 0, "First byte of COBS must be greater than 0");
//     uint32_t nextZero = packet[0] -1;
//     uint32_t crc = 0xFFFF;
//     uint32_t bytesLeft = packetLength - 2;
//     uint8_t * byte = &packet[1];

//     // COBS decode and calc crc
//     for (; bytesLeft--; nextZero--, byte++) {
//         if (nextZero == 0) {
//             nextZero = *byte;
//             *byte = 0;
//         }
//         crc = ((crc << 8) & 0xFFFF) ^ LUT[(crc >> 8) ^ *byte];
//     }
//     if (crc != 0) {
//         if (sp->rx.initialised) {
//             softAssert(0, "CRC failed");
//             sp->stats.rxCrcErrors++;
//         }
//         return -1;
//     }
//     if (nextZero != 0) {
//         if (sp->rx.initialised) { // TODO: could change to just ignore until first packet/ping
//             sp->stats.rxCobsDecodingErrors++;
//             softAssert(0, "COBS error for end length");
//         }
//         return -1;
//     }
//     return 0;
// }

// ====================== //
// Serial thread!

// For transferring IO data to buffers for the main thread
// Ensuring IO buses are promptly served

static bool spRequestReady(spInterface * sp) {
    // Check there is data to send
    if (QUEUE_EMPTY(sp->tx.requests.start, sp->tx.requests.end, SP_MAX_TX_REQUESTS)) {
        return false;
    }
    // If we've reached the end wait WRAP_WAIT_TIME_MS for an ack to come back before wrapping and retransmitting
    // We will continue transmitting when either new data is put on the tx queue incrementing the end
    // or if an ack never comes back
    if (sp->tx.requests.next == sp->tx.requests.end) {
        spTxRequestMetaData * startPacket = QUEUE_PEEK(sp->tx.requests.entries, sp->tx.requests.start);
        if (HAL_GetTick() >= startPacket->timeSent + wrapWaitTimeMs) {
            sp->tx.requests.next = sp->tx.requests.start;
            sp->stats.txWindowWrap++;
            // softAssert(0, "SP tx wrapping");
            return true;
        } else {
            return false;
        }
    }
    return true;
}

static uint8_t * spGetRequestToSend(spInterface * sp, uint32_t * length) {
    // Get request to send
    spTxRequestMetaData * request = QUEUE_PEEK(sp->tx.requests.entries, sp->tx.requests.next);
    request->timeSent = HAL_GetTick();
    sp->stats.egress.uavCanFrames++;
    sp->tx.requests.next  = incrAndWrap(sp->tx.requests.next, 1, SP_MAX_TX_REQUESTS);
    *length = request->length;
    return &sp->tx.requests.rawbuffer->data[request->head];
}

static uint32_t spFillInResponse(spInterface * sp, uint8_t * packet) {
    uint32_t length;
    // Pong takes precendence - as we use it to clear channel before initialising
    if (sp->tx.responses.pong) {
        sp->stats.ingress.pongs++;
        sp->tx.responses.pong = 0;
        packet[1] = SP_PONG;
        packet[2] = SP_PROTOCOL_VERSION;
        length = 2 + CRC_COBS_OVERHEAD;

    } else if (sp->tx.responses.initialised) {
        sp->tx.responses.initialised = 0;
        if (sp->tx.responses.uninitialised) {
            // Shouldn't really happen but if it does clear both flags and the latest one should be set later
            sp->tx.responses.uninitialised = 0;
            softAssert(0, "Both init and uninit response set");
            return 0;
        } else {
            sp->stats.ingress.initialised++;
            packet[1] = SP_INITIALISED;
            length = 1 + CRC_COBS_OVERHEAD;
        }
    } else if (sp->tx.responses.uninitialised) {
        sp->tx.responses.uninitialised = 0;
        sp->stats.ingress.uninitialised++;
        packet[1] = SP_UNINITIALISED;
        length = 1 + CRC_COBS_OVERHEAD;
    } else if (sp->tx.responses.ack && sp->tx.initialised) {
        // softAssert(sp->tx.initialised, "Sending ack when not initialised");
        sp->stats.ingress.uavCanFramesAcked += decrAndWrap(sp->rx.nextExpectedSequenceNumber, sp->rx.lastAckedSequenceNumber, SP_MAX_SEQUENCE_NUMBER);
        softAssert(sp->stats.ingress.uavCanFramesAcked <= sp->stats.ingress.uavCanFrames, "More frames acked than sent");
        sp->rx.lastAckedSequenceNumber = sp->rx.nextExpectedSequenceNumber;
        sp->tx.responses.ack = 0;
        packet[1] = SP_ACKNOWLEDGE;
        packet[2] = decrAndWrap(sp->rx.nextExpectedSequenceNumber, 1, SP_MAX_SEQUENCE_NUMBER);
        length = 2 + CRC_COBS_OVERHEAD;
    } else {
        return 0;
    }
    crcAndCobsEncodeData(packet, length);
    return length;
}

// Doesn't matter if this is inefficient - not normally called
static uint32_t spFillInInitialisationRequest(spInterface * sp, uint8_t * data) {
    tPacketLength length;
    // Keep sending PINGS until we've received enough PONGs to know the channel has been completely flushed of any old packets
    if (sp->tx.pongsReceived <= SP_MAX_PACKETS_IN_CHANNEL) {
        sp->stats.egress.pings++;
        data[1] = SP_PING;
        length = 1 + CRC_COBS_OVERHEAD;
    } else {
        sp->stats.egress.initialises++;
        // Once channel is flushed keep sending initialise packets until we receive an initialised response from other end
        data[1] = SP_INITIALISE;
        data[2] = SP_PROTOCOL_VERSION;
        data[3] = sp->tx.requests.startSequenceNumber;
        length = 3 + CRC_COBS_OVERHEAD;
    }
    crcAndCobsEncodeData(data, length);
    return length;
}

// We leave 6B of space at start of every tx frame and then fill in responses every time
// Called by timer every 25us - 8B at 3Mbps
// Needs to be fairly efficient
void spProcessTx(spInterface * sp) {
    uint8_t * data;
    uint32_t length = 0;
    bool validResponse = sp->tx.responses.ack || sp->tx.responses.initialised || sp->tx.responses.uninitialised || sp->tx.responses.pong;

    if (!sp->tx.initialised) {
        // Send an init packet
        sp->tx.cycleSinceLastInit++;
        if (sp->tx.cycleSinceLastInit == 5) {
            sp->tx.cycleSinceLastInit = 0; // TODO: better way to limit bandwidth?
            // Create a request
            data = &sp->tx.smallPacketPtr[0];
            uint32_t requestLength = spFillInInitialisationRequest(sp, data);
            // Then append a response if necessary
            uint32_t responseLength = 0;
            if (validResponse) {
                responseLength = spFillInResponse(sp, &data[requestLength]);
            }
            length = requestLength + responseLength;

        } else {
            return;
        }
        
    } else if (spRequestReady(sp)) {
        data = spGetRequestToSend(sp, &length);
        
        // We allocate bytes at the start of the request packet for a response
        if (!validResponse) {
            // If we don't need a response then change the length and start ptr
            length -= SP_MAX_RESPONSE_PACKET_SIZE;
            data = &data[SP_MAX_RESPONSE_PACKET_SIZE];
        } else {
            // If we need a response fill it in
            uint8_t tmpResponse[SP_MAX_RESPONSE_PACKET_SIZE];
            uint32_t responseLength = spFillInResponse(sp, tmpResponse);
            // Move the data pointer to the start of the response
            length -= (SP_MAX_RESPONSE_PACKET_SIZE - responseLength);
            data = &data[SP_MAX_RESPONSE_PACKET_SIZE - responseLength];
            memcpy(data, tmpResponse, responseLength);
        }

    } else if(validResponse) {
        length = spFillInResponse(sp, &sp->tx.smallPacketPtr[0]);
        data = sp->tx.smallPacketPtr;

    } else {
        return; // Nothing to do
    }

    // Send data
    if (sp->transmitFn(data, length) < 0) {
        // Shouldn't ever happen
        // Doesn't actually matter - protocol can handle requests and responses being dropped
        sp->stats.uartFailedToTransmit++;
        softAssert(0, "Uart failed to transmit");
    }
}


// ====================== //
// Main thread Tx side

// Only updates the start pointers - thread safe from spAddToRequests
static void spAcknowledgeRequest(spInterface * sp, uint32_t sequenceNumber) {
    uint32_t queueLength = QUEUE_LENGTH(sp->tx.requests.startSequenceNumber, sp->tx.requests.endSequenceNumber, SP_MAX_SEQUENCE_NUMBER);
    int numPacketsToRemove = decrAndWrap(sequenceNumber, sp->tx.requests.startSequenceNumber, SP_MAX_SEQUENCE_NUMBER);
    
    numPacketsToRemove += 1;
    if (numPacketsToRemove <= queueLength) {
        sp->stats.egress.uavCanFramesAcked += numPacketsToRemove;
        for (uint32_t i=0; i<numPacketsToRemove; i++) {
            QUEUE_POP(sp->tx.requests.start, sp->tx.requests.end, SP_MAX_TX_REQUESTS);
        }
        sp->tx.requests.startSequenceNumber = incrAndWrap(sp->tx.requests.startSequenceNumber, numPacketsToRemove, SP_MAX_SEQUENCE_NUMBER);
        sp->tx.requests.rawbuffer->start = sp->tx.requests.entries[sp->tx.requests.start].head;
    }
}

static bool spRequestBufferHasSpace(spInterface * sp, uint32_t length) {
    softAssert(length < 256, "Exceeded max packet length"); // Shouldn't happen
    if (QUEUE_FULL(sp->tx.requests.start, sp->tx.requests.end, SP_MAX_TX_REQUESTS)) {
        //softAssert(0, "Request metadata is full");
        sp->stats.txRequestsMetaDataFull++;
        return 0;
    }

    bool bufferHasSpace;
    sCircularBuffer * buffer = sp->tx.requests.rawbuffer;
    uint32_t fullLength = length + SP_MAX_RESPONSE_PACKET_SIZE;
    if (buffer->end > buffer->start) {
        // Not wrapped so check end
        uint32_t remainingEndSpace = buffer->size - buffer->end;
        bufferHasSpace = (fullLength < remainingEndSpace) ||(fullLength < buffer->start);
    } else {
        // Wrapped
        uint32_t remainingSpace = buffer->start - buffer->end - 1;
        bufferHasSpace = (fullLength < remainingSpace);
    }
    if (!bufferHasSpace) {
        //softAssert(0, "Request buffer is full");
        sp->stats.txRequestDataFull++;
        return 0;
    }
    return 1;
}

// NOTE: think carefully about threads calling this!
// Should only be called by main thread - only changes end pointers
static void spAddToRequests(spInterface * sp, uint8_t * packet, tPacketLength length) {
    sCircularBuffer * buffer = sp->tx.requests.rawbuffer;
    uint16_t packetPtr;
    uint32_t fullLength = length + SP_MAX_RESPONSE_PACKET_SIZE;
    // Ensure frames are always contiguous
    if (buffer->end > buffer->start) {
        // Not wrapped so check end
        uint32_t remainingEndSpace = buffer->size - buffer->end;
        if (fullLength < remainingEndSpace){
            packetPtr = buffer->end;
        } else {
            softAssert(length < buffer->start, "");
            packetPtr = 0;
        }
    } else {
        // Wrapped
        uint32_t remainingSpace = buffer->start - buffer->end - 1;
        if (fullLength < remainingSpace) {
            packetPtr = buffer->end;
        } else {
            softAssert(0, "");
            packetPtr = 0;
        }
    }
    buffer->end = packetPtr + fullLength; // Must be atomic 
    memcpy(&sp->tx.requests.rawbuffer->data[packetPtr + SP_MAX_RESPONSE_PACKET_SIZE], packet, length);
    spTxRequestMetaData metaData = {packetPtr, fullLength, 0};
    QUEUE_APPEND(sp->tx.requests.entries, sp->tx.requests.start, sp->tx.requests.end, SP_MAX_TX_REQUESTS, metaData);
    sp->tx.requests.endSequenceNumber = incrAndWrap(sp->tx.requests.endSequenceNumber, 1, SP_MAX_SEQUENCE_NUMBER);
}

static int8_t spGenericSendUavcan(spInterface * sp, uint8_t uavcanHeaderBytes[], uint8_t * transferId,  const void* payload, uint16_t payloadLength) {
    if (!sp->tx.initialised)
        return 1; // Not an error so just return 1
    uint8_t packet[SP_MAX_PACKET_SIZE];
    uint8_t fullLength = payloadLength + SP_REQUEST_OVERHEAD + 5;
    // softAssert(spRequestBufferHasSpace(fullLength), "Tx buffer full");
    if (spRequestBufferHasSpace(sp, fullLength)) {
        // packet[0] will be used for cobs
        packet[1] = SP_UAVCAN;
        packet[2] = *transferId;
        packet[3] = uavcanHeaderBytes[0];
        packet[4] = uavcanHeaderBytes[1];
        packet[5] = uavcanHeaderBytes[2];
        packet[6] = uavcanHeaderBytes[3];
        // packBroadcastUavCanHeader(&packet[4], sourceNodeId, dataTypeId, priority);
        memcpy(&packet[7], payload, payloadLength);
        packet[payloadLength+7] = sp->tx.requests.endSequenceNumber;
        crcAndCobsEncodeData(packet, fullLength);
        spAddToRequests(sp, packet, fullLength);
        (*transferId)++;
        if (*transferId >= 32)
            *transferId = 0;
        return 1; // Needs to somewhat match canardBroadcast which returns number of canframes enqueued
    } else {
        return -1;
    }
}

int8_t spSendUavcanBroadcast(spInterface * sp, uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength) {
    uint8_t uavcanHeaderBytes[4];
    packBroadcastUavCanHeader(uavcanHeaderBytes, sourceNodeId, dataTypeId, priority);
    return spGenericSendUavcan(sp, uavcanHeaderBytes, transferId,  payload, payloadLength);
}

int8_t spSendUavcanRequestResponse(spInterface * sp, uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId,
        uint8_t priority, uint8_t * transferId, const void* payload, uint16_t payloadLength) {
    uint8_t uavcanHeaderBytes[4];
    packRequestResponseUavCanHeader(uavcanHeaderBytes, sourceNodeId, destNodeId, request, dataTypeId, priority);
    return spGenericSendUavcan(sp, uavcanHeaderBytes, transferId,  payload, payloadLength);
}


// ====================== //
// Rx side

void spProcessRxUavCanFrame(spInterface * sp, uint8_t * data, uint16_t length) {
    CanardRxTransfer transfer;
    bool isBroadcast;
    uint8_t sourceNodeId;
    uint8_t destNodeId;
    uint8_t priority;
    uint16_t dataTypeId;
    bool request;

    uint8_t transferId = data[0];
    unpackUavCanHeader(&data[1], &isBroadcast, &sourceNodeId, &destNodeId, &request, &dataTypeId, &priority);
    uint8_t * payload = &data[5];
    uint8_t payloadLength = length-5;

    transfer.transfer_id = transferId;
    transfer.transfer_type = isBroadcast ? CanardTransferTypeBroadcast : (request ? CanardTransferTypeRequest : CanardTransferTypeResponse);
    transfer.payload_len = payloadLength;
    transfer.data_type_id = dataTypeId;
    transfer.priority = priority;
    transfer.source_node_id = sourceNodeId;

    canOnTransferReceived(true, &transfer, destNodeId, payload, payloadLength);
}

static void spProcessRxFrame(spInterface * sp, uint8_t * data, uint8_t dataLength) {
    uint8_t command = data[0];

    if (IS_SP_REQUEST(command)) {
        
        if (command == SP_UAVCAN) {
            sp->stats.ingress.uavCanFrames++;
            uint8_t SequenceNumber = data[dataLength-1]; // Sequence number is the last byte of the packet, just before the CRC
            sp->tx.responses.ack = 1; // Always ack
            if (sp->rx.nextExpectedSequenceNumber == SequenceNumber) {
                CEXCEPTION_T e;
                Try {
                    spProcessRxUavCanFrame(sp, &data[1], dataLength-2);
                    sp->rx.nextExpectedSequenceNumber = incrAndWrap(sp->rx.nextExpectedSequenceNumber, 1, SP_MAX_SEQUENCE_NUMBER);
                } Catch(e) {
                    sp->stats.corruptedFrameData++;
                }
            } else {
                sp->stats.notNextSequenceNum++;
            }

        } else if (command == SP_PING) {
            sp->tx.responses.pong = 1;
            sp->stats.ingress.pings++;

        } else if (command == SP_INITIALISE) {
            sp->stats.ingress.initialises++;
            uint8_t protocolVersion = data[1];
            hardAssert(protocolVersion == SP_PROTOCOL_VERSION, "Unsupported protocol version");
            sp->rx.nextExpectedSequenceNumber = data[2];
            sp->rx.lastAckedSequenceNumber = data[2];
            sp->rx.initialised = 1;
            sp->tx.responses.initialised = 1;

        } else if (sp->rx.initialised == 0) {
            sp->tx.responses.uninitialised = 1;

        } else {
            sp->tx.responses.ack = 1;
            sp->stats.rxInvalidCommand++;
            softAssert(0, "Invalid rx command"); // Should almost never happen - means it fooled the CRC - or different protocol being used
        }
    } else {
        if (command == SP_ACKNOWLEDGE) {
            uint8_t lastAcceptedSequenceNumber = data[1];
            spAcknowledgeRequest(sp, lastAcceptedSequenceNumber);
            
        } else if (command == SP_UNINITIALISED) {
            sp->stats.egress.uninitialised++;
            if (sp->tx.initialised) {
                sp->tx.initialised = 0;
                sp->tx.pongsReceived = 0;
            }
        } else if (command == SP_PONG) {
            sp->tx.pongsReceived++;
            sp->stats.egress.pongs++;
            // sp->tx.cyclesSinceInitialisationResponse = 0;

        } else if (command == SP_INITIALISED) {
            sp->stats.egress.initialised++;
            // sp->tx.cyclesSinceInitialisationResponse = 0;
            if (sp->tx.pongsReceived > SP_MAX_PACKETS_IN_CHANNEL) {
                sp->tx.initialised = 1;
            }

        } else {
            // Invalid response - doesn't matter the next response will subsume this one
            sp->stats.rxInvalidCommand++;
            softAssert(0, "Invalid rx command");
        }
    }
}

// Return num packets read -1, 0, or 1 (-1 means error)
static int8_t spReadIncomingForNewFrame(spInterface * sp, uint8_t * packet, uint32_t * packetLength) {
    *packetLength = 0;
    uint16_t dmaEnd = sp->rxTailFn();

    // Empty
    if (sp->rx.buffer->start == dmaEnd)
        return 0;

    // TODO put back
    //hardAssert((dmaEnd < sp->rx.buffer->size), "Unexpected dma offset");
    if (!(dmaEnd < sp->rx.buffer->size))
        return 0;

    const uint32_t size = sp->rx.buffer->size;
    uint32_t start = sp->rx.buffer->start;
    uint32_t end = dmaEnd;
    if (end < start) {
        end = size;
    }
    uint32_t maxBytes = end - start;
    // Set the end to take into account the max packet size as well (we don't overflow the packet)
    maxBytes = MIN(maxBytes, (SP_MAX_PACKET_SIZE - sp->rx.tmpPacketLength));

    bool foundDelimiter = 0;
    uint8_t * buffer = (uint8_t *) &sp->rx.buffer->data[start];
    uint8_t * packetPtr  = &packet[sp->rx.tmpPacketLength];
    uint32_t nextZero = sp->rx.nextZero;
    uint32_t crc = sp->rx.crc;
    uint32_t i=0;

    if (sp->rx.tmpPacketLength == 0) {
        nextZero = buffer[0];
        crc = 0xFFFF;
        i++;
    }

    while (i < maxBytes) {
        uint32_t byte = buffer[i];
        // Check for frame delimiter
        if (byte == COBS_DELIMITER_BYTE) {
        	// softAssert(crc == 0, "CRC failed");
            foundDelimiter =1;
            i++;
            break;
        }
        // COBS
        nextZero--;
        if (nextZero == 0) {
            nextZero = byte;
            byte = 0;
        }
        // CRC
        crc = ((crc << 8) & 0xFFFF) ^ LUT[(crc >> 8) ^ byte];
        // Copy to packet buffer
        packetPtr[i] =  byte;
        i++;

    }
    // Save the state as we might have finished mid-frame
    sp->rx.tmpPacketLength += i;
    sp->rx.crc = crc;
    sp->rx.nextZero = nextZero;

    // Update the start
    start += i;
    if (start == size)
        start = 0;
    sp->rx.buffer->start = start; // Must be atomic for thread safety

    // Ensure we never overflow the temporary packet if the incoming data is corrupted
    // If exceeded max packet then reset the packet length so it starts writing into
    // the beginning of the packet again but record that it needs to be discarded
    if (sp->rx.tmpPacketLength >= SP_MAX_PACKET_SIZE) {
        sp->rx.tmpPacketLength = 0;
        sp->rx.discarding = 1;
    }

    // Found packet end
    if (foundDelimiter) {
        *packetLength = sp->rx.tmpPacketLength;
        sp->rx.tmpPacketLength = 0;

        if (sp->rx.discarding) {
            // softAssert(0, "Invalid rx packet");
            sp->rx.discarding = 0;
            *packetLength = 0;
            sp->stats.rxInvalidPacketLength++;
            return -1;
        } else if (crc != 0) {
            if (sp->rx.initialised) {
                *packetLength = 0;
                // softAssert(0, "CRC failed");
                sp->stats.rxCrcErrors++;
            }
            return -1;
        } else if (nextZero != 1) {
            if (sp->rx.initialised) {
                *packetLength = 0;
                // softAssert(0, "COBS decoding error");
                sp->stats.rxCobsDecodingErrors++;
            }
            return -1;
        }
        return 1;
    }
    return 0;
}

static void spRespondToInvalidFrame(spInterface * sp) {
    if (sp->rx.initialised) {
        sp->tx.responses.ack = 1;
    } else {
        sp->tx.responses.uninitialised = 1;
    }
}

bool spProcessRx(spInterface * sp) {
    bool packetsReceived = false;
    uint32_t packetLength;

    // Process all the frame in the rx buffer
    while (1) {
        // Check the UART buffer
        int8_t res = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);

        // Break if no new packet
        if ((res == 0) || (packetLength <= 1)) {
            return packetsReceived;
        } else if (res < 0) {
            spRespondToInvalidFrame(sp);
        } else {
            res = 0;
            if (res < 0) {
                spRespondToInvalidFrame(sp);
            } else {
                packetsReceived = true;
                uint8_t * data = &sp->rx.packet[1];
                uint32_t dataLength = packetLength - CRC_COBS_OVERHEAD;
                spProcessRxFrame(sp, data, dataLength);
            }
        }
    }
}

bool spInitialised(spInterface * sp) {
    return (sp->tx.initialised && sp->rx.initialised);
}

void spInitDrv(spInterface ** sp, tTransmitFn * transmitFn, tRxTailFn * rxTailFn, sCircularBuffer * txBuffer, sCircularBuffer * rxBuffer, uint8_t * smallPacketPtr) {
    if (*sp == NULL) {
        *sp = (spInterface *) malloc(sizeof(spInterface));
        hardAssert(*sp != NULL, "Out of ram!");
    }
    memset(*sp, 0, sizeof(spInterface));
    (*sp)->transmitFn = transmitFn;
    (*sp)->rxTailFn = rxTailFn;
    (*sp)->tx.requests.rawbuffer = txBuffer;
    (*sp)->rx.buffer = rxBuffer;
    (*sp)->tx.smallPacketPtr = smallPacketPtr;
}

#endif //ENABLE_SERIAL_PROTOCOL_MODULE




// =============================== //
#ifdef TESTING_SERIAL_PROTOCOL

uint8_t testSpSmallPacketData[SP_MAX_INIT_REQUEST_PACKET_SIZE + SP_MAX_RESPONSE_PACKET_SIZE] = {};
sCircularBuffer testSpTxBuffer;
sCircularBuffer testSpRxBuffer;

int testSpTransmitFn(uint8_t * data, uint32_t length) {
    appendBuffer(&testSpRxBuffer, data, length);
    return 0;
}
uint32_t testSpRxTailFn() {
    return testSpRxBuffer.end;
}
spInterface * testSpPtr;

// // data: 5 frames - made up frames 
#define TEST_SP_NUM_PACKETS 5
uint8_t testSpRxFrameData[] = {
    0x0, SP_UAVCAN,      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0, 0x0, // UAVCAN publish fields
    0x0, SP_ACKNOWLEDGE, 0x01, 0x0, 0x0, 0x0, // Ack
    0x0, SP_INITIALISED,       0x0, 0x0, 0x0, // Initialised
    0x0, SP_UAVCAN,      0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0, 0x0,// UAVCAN set fields
    0x0, SP_ACKNOWLEDGE, 0x03, 0x0, 0x0, 0x0// Ack
};
uint32_t testSpRxPacketLengths[TEST_SP_NUM_PACKETS] = {12, 6, 5, 12, 6};

#define TEST_SP_RX_BUFFER_SIZE 1024
#define TEST_SP_TX_BUFFER_SIZE 1024
uint8_t testSpTxData[TEST_SP_RX_BUFFER_SIZE] = {};
uint8_t testSpRxData[TEST_SP_TX_BUFFER_SIZE] = {}; // Written to by DMA

void testSpRxReadingNormalFrames(spInterface * sp, uint32_t pos) {
    uint32_t packetLength;
    // Test reading all data in one go / split over the buffer
    testSpRxBuffer.start = pos;
    testSpRxBuffer.end = pos;
    appendBuffer(&testSpRxBuffer, testSpRxFrameData, sizeof(testSpRxFrameData));
    for (uint32_t i=0; i<TEST_SP_NUM_PACKETS; i++) {
        int8_t result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
        if (result == 0) {
            // Call again for the buffer wrapped part
            result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
        }
        hardAssert(result == 1, "Failed to read frame");
        hardAssert(packetLength == testSpRxPacketLengths[i], "");
    }

    // Test reading data split over multiple reads
    for (uint32_t offset=1; offset<sizeof(testSpRxFrameData); offset++) {
        testSpRxBuffer.start = pos;
        testSpRxBuffer.end = pos;
        uint32_t packet=0;
        for (uint32_t read=0; read<2; read++) {
            if (read == 0) {
                // Add data up to offset
                appendBuffer(&testSpRxBuffer, &testSpRxFrameData[0], offset);
            } else {
                // Add data from pos
                appendBuffer(&testSpRxBuffer, &testSpRxFrameData[offset], sizeof(testSpRxFrameData)-offset);
            }
            // Call twice in case it's wrapped
            for (uint32_t try=0; try<2; try++) {
                while (1) {
                    int8_t result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
                    if (result == 0)
                        break;
                    hardAssert(packetLength == testSpRxPacketLengths[packet], "");
                    packet++;
                }
            }
        }
        hardAssert(packet == TEST_SP_NUM_PACKETS, "");
    }
}

void testSpRxReadingBadFrames(spInterface * sp, uint32_t pos) {
    uint32_t packetLength;
    bool prevRxInitialised = sp->rx.initialised;
    sp->rx.initialised = true; // Otherwise it ignores the errors

    // Test bad COBS
    {
        uint8_t tmpFrame[20];
        memset(tmpFrame, 0xFF, 20);
        testSpRxBuffer.start = pos;
        testSpRxBuffer.end = pos;
        crcAndCobsEncodeData(&tmpFrame[0], sizeof(tmpFrame));
        tmpFrame[0] = !tmpFrame[0];
        appendBuffer(&testSpRxBuffer, tmpFrame, sizeof(tmpFrame));
        // Call up to the 2 times for wrapped buffer and max length read
        int8_t result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
        if (result == 0) {
            // Call again for the buffer wrapped part
            result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
        }
        hardAssert(result == -1, "");
        hardAssert(packetLength == 0, "");
        hardAssert(sp->stats.rxCobsDecodingErrors == 1, "");
        sp->stats.rxCobsDecodingErrors = 0;
    }

    // Test bad CRC
    {
        uint8_t tmpFrame[20];
        memset(tmpFrame, 0xFF, 20);
        testSpRxBuffer.start = pos;
        testSpRxBuffer.end = pos;
        crcAndCobsEncodeData(&tmpFrame[0], sizeof(tmpFrame));
        tmpFrame[18] = !tmpFrame[18];
        appendBuffer(&testSpRxBuffer, tmpFrame, sizeof(tmpFrame));
        // Call up to the 2 times for wrapped buffer and max length read
        int8_t result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
        if (result == 0) {
            // Call again for the buffer wrapped part
            result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
        }
        hardAssert(result == -1, "");
        hardAssert(packetLength == 0, "");
        hardAssert(sp->stats.rxCrcErrors == 1, "");
        sp->stats.rxCrcErrors = 0;
    }

    // Test invalid packet length
    {
        uint8_t tmpFrame[SP_MAX_PACKET_SIZE+1 + CRC_COBS_OVERHEAD] = {};
        testSpRxBuffer.start = pos;
        testSpRxBuffer.end = pos;
        crcAndCobsEncodeData(&tmpFrame[0], sizeof(tmpFrame));
        appendBuffer(&testSpRxBuffer, tmpFrame, sizeof(tmpFrame));
        // Call up to the 3 times for wrapped buffer and max length read
        volatile int8_t result;
        for (uint32_t i=0; i<3; i++) {
            result = spReadIncomingForNewFrame(sp, sp->rx.packet, &packetLength);
            if (result != 0)
                break;
        }
        hardAssert(result == -1, "");
        hardAssert(packetLength == 0, "");
        hardAssert(sp->stats.rxInvalidPacketLength == 1, "");
        sp->stats.rxInvalidPacketLength = 0;
    }
    sp->rx.initialised = prevRxInitialised;
}

void testSpRxReadingFrames(spInterface * sp) {
    // Test with frames across all boundaries
    for (uint32_t pos=TEST_SP_RX_BUFFER_SIZE - 200; pos<TEST_SP_RX_BUFFER_SIZE; pos++) {
        testSpRxReadingNormalFrames(sp, pos);
        testSpRxReadingBadFrames(sp, pos);
    }
}

uint8_t testBrightness = 0;
uint8_t testShow = 0;
uint8_t pixelBuffer[60 * 3];
sFieldInfoEntry testFieldTable[] = {
	{ &testBrightness , "brightness",   100,        1,  AF_FIELD_TYPE_UINT,     1,   DONT_REPLY_TO_SET_FLAG, NULL, NULL, NULL},
	{ &pixelBuffer    , "led_colour",   101,       60,  AF_FIELD_TYPE_UINT,     3,   DONT_REPLY_TO_SET_FLAG, NULL, NULL, NULL},
	{ &testShow       , "show",         101 + 60,   1,  AF_FIELD_TYPE_BOOLEAN,  1,   DONT_REPLY_TO_SET_FLAG, NULL, NULL, NULL}
};
const uint32_t testFieldTableSize = sizeof(testFieldTable)/sizeof(sFieldInfoEntry);

// TODO: move
#include "can.h"
spInterface * testSpDrv;

// Test full system
void testSpFullSerialSystem(spInterface * sp) {
    uint32_t count = 0;
    // Initialise
    while (count < 5000) {
        spProcessTx(sp);
        spProcessRx(sp);
        count++;
        if (sp->rx.initialised && sp->tx.initialised) {
            break;
        }
    }
    softAssert(sp->rx.initialised && sp->tx.initialised, "");

    // Test one frame
    uint8_t newPixelBuffer[60 * 3];
    memset(newPixelBuffer, 0xAA, sizeof(newPixelBuffer));
    setFields(getLocalNodeId(), 101, 160, newPixelBuffer, sizeof(newPixelBuffer));
    while (count < 100000 && sp->stats.ingress.uavCanFrames != 1) {
        spProcessTx(sp);
        spProcessRx(sp);
        count++;
    }
    softAssert(sp->stats.ingress.uavCanFrames == 1, "");
    for (uint32_t i=0; i<sizeof(newPixelBuffer); i++) {
        softAssert(pixelBuffer[i] == newPixelBuffer[i], "");
    }


    volatile uint32_t corruptions = 0;
    uint32_t j=0;
    uint32_t pixel = 0;
    count = 0;
    while (j < 100) {
        while (bufferHasSpace(&testSpTxBuffer, 256)) {
            setFields(getLocalNodeId(), 101 + pixel, 101 + pixel + 4, &newPixelBuffer[pixel], 5*3);
            pixel += 5;
            if (pixel >= 60) {
                memset(newPixelBuffer, j, sizeof(newPixelBuffer));
                j++;
                pixel = 0;
            }
        }

        spProcessTx(sp);

        // Corrupt data randomly
        if (count % 30 == 0) {
            uint32_t length = bufferLength(sp->rx.buffer);
            if (length) {
                uint32_t randNum = pixel % length;
                uint32_t offset = (sp->rx.buffer->start + randNum) % sp->rx.buffer->size;
                testSpRxData[offset]++;
                corruptions++;
            }
        }

        spProcessRx(sp);
        count++;
    }
    volatile uint32_t drainCount = 0;
    while (QUEUE_LENGTH(sp->tx.requests.start, sp->tx.requests.end, SP_MAX_TX_REQUESTS) && drainCount < 1000000) {
        spProcessTx(sp);
        spProcessRx(sp);
        drainCount++;
    }
    volatile uint32_t length = QUEUE_LENGTH(sp->tx.requests.start, sp->tx.requests.end, SP_MAX_TX_REQUESTS);
    softAssert(length == 0, "");
    softAssert(sp->stats.ingress.uavCanFrames > 1000, "");
    for (uint32_t i=0; i<sizeof(newPixelBuffer); i++) {
        softAssert(pixelBuffer[i] == 98, "");
    }
}

extern void testLoopbackCan();
extern uint16_t testUavRxCount();
extern uint16_t testUavTxCount();

void testSpSerialToCan(spInterface * sp) {
    // Create a packet to set a fake node
    uint8_t newPixelBuffer[10] = {};
    uint8_t fakeNodeId = 12;
    setFields(fakeNodeId, 101, 110, newPixelBuffer, 3*10);

    // The CAN and the serial link are both in loopback
    // So a frame to a foreign node should get passed around forever
    uint32_t count = 0;
    while (count < 1000) {
        testLoopbackCan();
        loopUavCan();
        spProcessTx(sp);
        spProcessRx(sp);
        count++;
    }
    volatile uint16_t rxCount = testUavRxCount();
    volatile uint16_t txCount = testUavTxCount();
    softAssert(rxCount > 10, "");
    softAssert(txCount > 10, "");
}

void testSp() {
    uint32_t index = 0;
    for (uint32_t i=0; i<TEST_SP_NUM_PACKETS; i++) {
        crcAndCobsEncodeData(&testSpRxFrameData[index], testSpRxPacketLengths[i]);
        index += testSpRxPacketLengths[i];
    }

    testSpTxBuffer.size = sizeof(testSpTxData);
    testSpTxBuffer.data = testSpTxData;
    testSpRxBuffer.size = sizeof(testSpRxData);
    testSpRxBuffer.data = testSpRxData;
    spInitDrv(&testSpPtr, testSpTransmitFn, testSpRxTailFn, &testSpTxBuffer, &testSpRxBuffer, testSpSmallPacketData);

    testSpRxReadingFrames(testSpPtr);

    // Hack up the system for the tests
    wrapWaitTimeMs = 1; // So we don't wait ages for retransmissions
    testSpDrv = testSpPtr; // For the serlink to transmit on the fake loop serial link 
    setupCan();
    testSetNodeId(124); // So the CAN layer will actually transmit
    addComponentFieldTableToGlobalTable(testFieldTable, testFieldTableSize); // Add fields for the afFieldProtocol

    testSpFullSerialSystem(testSpPtr);
    
    testSpSerialToCan(testSpPtr);

    // Set them setup back to normal - Still not normal!
    // Free testSpDrv
    // testSetNodeId(0)
    // wrapWaitTimeMs = WRAP_WAIT_TIME_MS;

    softAssert(0, "");
}

#endif // TESTING_SERIAL_PROTOCOL
