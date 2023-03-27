/* Copyright (c) 2021 Abstract Foundry Limited */

#include "project.h"

#ifdef ENABLE_SERIAL_PROTOCOL_MODULE

#define SP_MAX_PACKET_SIZE 255
#define CRC_COBS_OVERHEAD 4
#define SP_MAX_DATA_LENGTH (SP_MAX_PACKET_SIZE - CRC_COBS_OVERHEAD)
#define SP_MAX_RESPONSE_LENGTH 2
#define SP_MAX_RESPONSE_PACKET_SIZE (SP_MAX_RESPONSE_LENGTH + CRC_COBS_OVERHEAD)
#define SP_MAX_INIT_REQUEST_LENGTH 3
#define SP_MAX_INIT_REQUEST_PACKET_SIZE (SP_MAX_INIT_REQUEST_LENGTH + CRC_COBS_OVERHEAD)

typedef struct spInterface spInterface;
typedef bool (tTransmissionInProgressFn)();
typedef int (tTransmitFn)(uint8_t * data, uint32_t length);
typedef uint32_t (tRxTailFn)();

void spInitDrv(spInterface ** sp, tTransmitFn * transmitFn, tRxTailFn * rxTailFn, sCircularBuffer * txBuffer, sCircularBuffer * rxBuffer, uint8_t * smallPacketPtr);
bool spProcessRx(spInterface * sp);
void spProcessTx(spInterface * sp);
bool spInitialised(spInterface * sp);
int8_t spSendUavcanBroadcast(spInterface * sp, uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength);
int8_t spSendUavcanRequestResponse(spInterface * sp, uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId, uint8_t priority, uint8_t * transferId, const void* payload, uint16_t payloadLength);

#endif //ENABLE_SERIAL_PROTOCOL_MODULE
