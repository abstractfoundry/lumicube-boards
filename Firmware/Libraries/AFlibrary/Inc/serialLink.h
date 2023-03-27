/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"

#ifdef ENABLE_SERIAL_PROTOCOL_MODULE

#include "usbd_def.h"

void spSetup(TIM_HandleTypeDef * htim, UART_HandleTypeDef * uart, USBD_HandleTypeDef * usbDeviceFS);
void spProcess();
void spTimerCallback();

int16_t canBroadcastOverSerial(uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength);
int16_t canRequestResponseOverSerial(uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId, uint8_t priority, uint8_t * transferId, const void* payload, uint16_t payloadLength);

// int8_t sendUavcanBroadcastOverSerial(uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength);
// int8_t sendUavcanRequestResponseOverSerial(uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId, uint8_t priority, uint8_t * transferId, const void* payload, uint16_t payloadLength);

#endif // ENABLE_SERIAL_PROTOCOL_MODULE
