/* Copyright (c) 2021 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_SERIAL_PROTOCOL_MODULE

#include <stdint.h>
#include <assert.h>

#include "useful.h"
#include "afFieldProtocol.h"
#include "canard.h"
#include "can.h"
#include "usbd_cdc_if.h"
#include "serialProtocol.h"

// Setup
// =====
// USB - add to usbd_cdc_if.c
// in PRIVATE_FUNCTIONS_DECLARATION add
//     extern void usbRxCallback(uint8_t* Buf, uint32_t *Len);
// in CDC_Receive_FS(uint8_t* Buf, uint32_t *Len) call 
//     usbRxCallback(Buf, Len);


// Pi can end up with 10ms latency
// So need at 4.5Kb of buffer space for 3Mbps
#define SP_TX_DATA_BUFFER_LENGTH (1024 * 16)
#define SP_RX_DATA_BUFFER_LENGTH (1024 * 16)

typedef struct {
    // USB
    bool usbConnected;
    USBD_HandleTypeDef * usbDeviceFS;
    uint8_t usbTxData[SP_TX_DATA_BUFFER_LENGTH];
    uint8_t usbRxData[SP_RX_DATA_BUFFER_LENGTH];
    sCircularBuffer usbTxBuffer;
    sCircularBuffer usbRxBuffer;
    spInterface * spUsb;
    // Uart
    bool uartConnected;
    UART_HandleTypeDef * uartDevice;
    sCircularBuffer uartTxBuffer;
    sCircularBuffer uartRxBuffer;
    spInterface * spUart;
    uint8_t * uartTxDataPtr;
    uint32_t uartTxDataLength;
} spState;

spInterface * testSpDrv = NULL;

__attribute__ ((section(".dmadata")))
uint8_t uartTxData[SP_TX_DATA_BUFFER_LENGTH] = {};
__attribute__ ((section(".dmadata")))
uint8_t uartRxData[SP_RX_DATA_BUFFER_LENGTH] = {}; // Written to by DMA
__attribute__ ((section(".dmadata")))
uint8_t uartSmallPacketData[SP_MAX_INIT_REQUEST_PACKET_SIZE + SP_MAX_RESPONSE_PACKET_SIZE] = {};

uint8_t usbSmallPacketData[SP_MAX_INIT_REQUEST_PACKET_SIZE + SP_MAX_RESPONSE_PACKET_SIZE] = {};


static spState spDrv = {};

// ====================== //
// USB

void usbRxCallback(uint8_t* Buf, uint32_t *Len) {
    int8_t res = appendBuffer(&spDrv.usbRxBuffer, Buf, *Len);
    if (res != 0) {
        //softAssert(0, "Rx buffer full");
        return;
    }
    memset(Buf, '\0', *Len);   // clear the Buf also
}

static uint32_t usbRxTail() {
    return spDrv.usbRxBuffer.end;
}

static bool usbTransmissionInProgress() {
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*) spDrv.usbDeviceFS->pClassData;
    if (hcdc->TxState != 0){
        return 1;
    }
    return 0;
}

static int usbTransmit(uint8_t * data, uint32_t length) {
    return (CDC_Transmit_FS(data, length) != USBD_OK ? -1 : 0);
}

// ====================== //
// Uart

static uint32_t uartRxTail() {
    uint32_t dmaRegister = __HAL_DMA_GET_COUNTER(spDrv.uartDevice->hdmarx); //->Instance->NDTR;
    return (SP_RX_DATA_BUFFER_LENGTH - 1 - dmaRegister);
}

// TODO: is there an easier way to find out if uart is busy - if so remove this code and disable uart interrupt
static bool uartReady = true;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    uartReady = true;
}
// static bool uartTransmissionInProgress() {
//    return !uartReady;
//     // HAL_UART_IRQHandler(spDrv.uartDevice); // Work around issue with HAL_UART_Transmit_DMA (where it gets stuck in BUSY state)
//     // return spDrv.uartDevice->gState != HAL_UART_STATE_READY || spDrv.uartDevice->hdmatx->State != HAL_DMA_STATE_READY;
// }

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->ErrorCode != HAL_UART_ERROR_DMA && huart->ErrorCode != HAL_UART_ERROR_RTO)
        return;

	// Probably an issue with the DMA ram being in the DTCMRAM block which isn't allowed (check linker script)
    softAssert(0, "HAL_UART_ErrorCallback");
    uartReady = true;
}

static void uartTxContinue() {
    uint32_t length = MIN(spDrv.uartTxDataLength, 8);
    uartReady = false;
    // TODO: look into how slow this HAL call is - we could do bare metal calls instead
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(spDrv.uartDevice, spDrv.uartTxDataPtr, length);
    softAssert(status == HAL_OK, "");
    spDrv.uartTxDataLength -= length;
    spDrv.uartTxDataPtr += length;
}

static int uartTransmit(uint8_t * data, uint32_t length) {
    spDrv.uartTxDataPtr = data;
    spDrv.uartTxDataLength = length;
    uartTxContinue();
    return 0;
}

// ====================== //

void spSetup(TIM_HandleTypeDef * htim, UART_HandleTypeDef * uartDevice, USBD_HandleTypeDef * usbDeviceFS) {
    // Setup USB buffer and pointers
    spDrv.usbDeviceFS = usbDeviceFS;
    spDrv.usbTxBuffer.size = SP_TX_DATA_BUFFER_LENGTH;
    spDrv.usbRxBuffer.size = SP_RX_DATA_BUFFER_LENGTH;
    spDrv.usbTxBuffer.data = spDrv.usbTxData;
    spDrv.usbRxBuffer.data = spDrv.usbRxData;
    spInitDrv(&spDrv.spUsb, &usbTransmit, &usbRxTail, &spDrv.usbTxBuffer, &spDrv.usbRxBuffer, usbSmallPacketData);

    // Setup UART buffer and pointers
    spDrv.uartDevice = uartDevice;
    spDrv.uartTxBuffer.size = SP_TX_DATA_BUFFER_LENGTH;
    spDrv.uartRxBuffer.size = SP_RX_DATA_BUFFER_LENGTH;
    spDrv.uartTxBuffer.data = uartTxData;
    spDrv.uartRxBuffer.data = (uint8_t *) uartRxData;
    spInitDrv(&spDrv.spUart, &uartTransmit, &uartRxTail, &spDrv.uartTxBuffer, &spDrv.uartRxBuffer, uartSmallPacketData);

    // HAL_Delay(10);

    // Start uart rx dma
    softAssert(HAL_UART_Receive_DMA(uartDevice, uartRxData, SP_RX_DATA_BUFFER_LENGTH) == HAL_OK, "Failed to start serial link UART Rx DMA");

    // Setup timer to process Tx data
    softAssert(HAL_TIM_Base_Start_IT(htim) == HAL_OK, "Failed to start serial link Tx timer");
}

// Schedule a Tx DMA
void spTimerCallback() {
	if (spDrv.usbConnected) {
        if (!usbTransmissionInProgress()) {
		    spProcessTx(spDrv.spUsb);
        }
    }
	if (spDrv.uartConnected && uartReady) {
        // NOTE: we throttle the UART Tx to 160kBps (1.28Mbps) with 8B bursts as the Raspberry Pi can't keep up and it's buffers are 16B
        // That means this should be called every 50us
		if (spDrv.uartTxDataLength == 0) {
			spProcessTx(spDrv.spUart);
		} else {
            uartTxContinue();
        }
	}
}

// Needs to be called every 20ms at least
void spProcess() {
    //static uint32_t nextTime = -1;
    // Check that time passed is not too much
    uint32_t time = HAL_GetTick();
    //softAssert(time < nextTime, "Too long between spProcess calls");
    //nextTime = time + ((SP_RX_DATA_BUFFER_LENGTH * 8 * 1000 / 2) / 3000000); // Time taken for half the RX buffer to fill up at 3Mbps

    // Process Rx
    // Note: the Tx is processed by timer interrupt
    bool usbFrameReceived = spProcessRx(spDrv.spUsb);
    bool uartFrameReceived = spProcessRx(spDrv.spUart);

    // If we receive any frames then the connection is up (but will time out after a second)
    static uint32_t usbTimeout = 0;
    if (usbFrameReceived) {
        spDrv.usbConnected = true;
        usbTimeout = HAL_GetTick() + 1000;
    } else if (time > usbTimeout) {
        spDrv.usbConnected = false;
    }
    static uint32_t uartTimeout = 0;
    if (uartFrameReceived) {
        spDrv.uartConnected = true;
        uartTimeout = HAL_GetTick() + 1000;
    } else if (time > uartTimeout) {
        spDrv.uartConnected = false;
    }
}

int16_t canBroadcastOverSerial(uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength) {
    int16_t res1 = 0, res2 = 0;
    if (spDrv.usbConnected)
        res1 = spSendUavcanBroadcast(spDrv.spUsb, sourceNodeId, dataTypeId, priority, transferId,  payload, payloadLength);
    if (spDrv.uartConnected)
        res2 = spSendUavcanBroadcast(spDrv.spUart, sourceNodeId, dataTypeId, priority, transferId,  payload, payloadLength);
    if (res1 < 0 || res2 < 0)
        return -1;
    return 1;
}

int16_t canRequestResponseOverSerial(uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId,
        uint8_t priority, uint8_t * transferId, const void* payload, uint16_t payloadLength) {
    int16_t res1 = 0, res2 = 0, res3 = 0;
    if (spDrv.usbConnected)
        res1 = spSendUavcanRequestResponse(spDrv.spUsb, sourceNodeId, destNodeId, request, dataTypeId, priority, transferId, payload, payloadLength);
    if (spDrv.uartConnected)
        res2 = spSendUavcanRequestResponse(spDrv.spUart, sourceNodeId, destNodeId, request, dataTypeId, priority, transferId, payload, payloadLength);
    if (testSpDrv) {
        res3 = spSendUavcanRequestResponse(testSpDrv, sourceNodeId, destNodeId, request, dataTypeId, priority, transferId, payload, payloadLength);
    }
    if (res1 < 0 || res2 < 0|| res3 < 0)
        return -1;
    return 1;
}

#endif //ENABLE_SERIAL_PROTOCOL_MODULE


