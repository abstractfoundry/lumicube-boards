/* Copyright (c) 2020 Abstract Foundry Limited */

#ifndef APP_INC_CAN_H_
#define APP_INC_CAN_H_

#include "project.h"
#include <stdint.h>
#include "canard.h"

// typedef int8_t (canBroadcastFnType)(uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength);
// typedef int16_t (respondFunc)(CanardRxTransfer *transfer, uint8_t dataTypeId, const void* payload, uint16_t payloadLength);

uint64_t getMonotonicTimestampUSec();
void incrementTimestampUsec(uint32_t inc);

void setupCan();
void loopRawCan(); // Must be called every 100us
void loopUavCan(); // Must be called at least every 5ms
bool isNodeAllocated();
uint8_t getLocalNodeId();
// uint64_t getMonotonicTimestampUSec();

void canOnTransferReceived(bool canOverSerial, CanardRxTransfer * transfer, uint8_t destNodeId, const void* payload, uint16_t payloadLength);

// Always send
void broadcastFieldsPacket(uint8_t packet_bytes[], size_t num_packet_bytes);
// Sent only if below bandwidth
int8_t publishFieldPacketIfBelowBandwidth(uint8_t packet_bytes[], size_t num_packet_bytes);
void requestSetFields(uint8_t destNodeId, uint8_t packetBytes[], size_t numBytes);

bool allCanFramesSent();
void testSetNodeId(uint8_t nodeId);

#endif /* APP_INC_CAN_H_ */
