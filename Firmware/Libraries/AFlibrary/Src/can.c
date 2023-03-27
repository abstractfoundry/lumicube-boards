/* Copyright (c) 2020 Abstract Foundry Limited */

// #include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // for useconds_t

#include "project.h"
#include "useful.h"
#include "can.h"
#include "canard_stm32.h"

// The CAN clock needs to be 48MHz
// The prescalar is then 3
// The bit segment 1 is 13
// The bit segment 2 is 2
// The sync jump width is 1 // TODO: look at changing this to handle our low accuracy clocks
// For FD can:
//   Set RX and TX FIFO element size to 8 bytes
//   Set RX and TX FIFO elements Nbr to 16 entries 

// - Timer 13
// 	activated
// 	period 9600 (can)
// 	enable interrupt

// can1 - PB8, PB9
// 	disable setup code

#ifdef H7
    extern FDCAN_HandleTypeDef hfdcan1;
#endif

#ifdef ENABLE_SERIAL_PROTOCOL_MODULE
    #define USING_SERIAL_LINK 1
#else
    #define USING_SERIAL_LINK 0
#endif

// Calls to app.c
extern char * getBoardName();
extern char * getBoardNodeName();
extern bool boardTest();

// Calls to afFieldProtocol
extern void afProtocolProcessRx(uint8_t * data, uint8_t dataLength);
extern void handleMetaDataRequest(uint8_t * data, uint8_t dataLength, uint8_t ** responsePacket, uint16_t * responseLength);

// Calls to debug component
extern void setRdp(uint8_t value);
extern uint32_t readRdpValue();

// Calls to serial link if there is one
__attribute__((weak)) int16_t canBroadcastOverSerial(uint8_t sourceNodeId, uint16_t dataTypeId, uint8_t priority, uint8_t * transferId,  const void* payload, uint16_t payloadLength);
__attribute__((weak)) int16_t canRequestResponseOverSerial(uint8_t sourceNodeId, uint8_t destNodeId, bool request, uint8_t dataTypeId, uint8_t priority, uint8_t * transferId, const void* payload, uint16_t payloadLength);

// // Optional hooks
__attribute__((weak)) void hookCanFrameTransmitted(uint64_t timestamp, const CanardCANFrame *frame) {}
__attribute__((weak)) void hookCanFrameReceived(uint64_t timestamp, const CanardCANFrame *frame) {}

// Buffers from CAN peripheral to UAVCAN memory pool
// Needs to provide enough buffering to ensure Tx side never underflows and Rx side never overflows
// Minimum CAN frame size is 64 bits -> 15625 frames per second -> 64us per frame
// So buffer of 8 frames means it needs to be processed every 0.5ms
#define TX_CAN_BUFFER_SIZE 8
#define RX_CAN_BUFFER_SIZE 16

// 1024 bytes -> 8000 bits -> 8ms of data
#ifndef NUM_CANARD_MEMORY_POOL_BYTES
    #define NUM_CANARD_MEMORY_POOL_BYTES 1024
#endif

#define APP_VERSION_MAJOR                                               1
#define APP_VERSION_MINOR                                               1

#ifndef GIT_HASH
#define GIT_HASH                                                        0xBADC0FFE // TODO: Normally this should be queried from the VCS when building the firmware
#endif

// Anonomous service data types (for broadcasts when source ID is 0)
// (Only 2 bits so only 0-3)
#define UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID                          1
#define AF_TEST_BOARD_RESULTS_DATA_TYPE_ID                              3
// Service data types
#define UAVCAN_GET_NODE_INFO_DATA_TYPE_ID                               1
#define AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_ID                        200
#define AF_GET_PREFERRED_NAME_DATA_TYPE_ID                              202
#define AF_GET_FIELD_META_DATA_DATA_TYPE_ID                             204
#define AF_SET_FIELDS_DATA_TYPE_ID                                      216 // -> 231 - We might need multiple ids to have more than 32 tids in flight
// Broadcast data types
#define UAVCAN_NODE_STATUS_DATA_TYPE_ID                                 341
#define AF_PUBLISHED_FIELDS_DATA_TYPE_ID                                20000
#define AF_TEST_BOARD_DATA_TYPE_ID                                      20998
#define AF_PROTECT_BOARD_DATA_TYPE_ID                                   20999

// TODO: get rid of data type signatures
#define UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_SIGNATURE                   0x0b2a812620a11d40
#define AF_TEST_BOARD_RESULTS_DATA_TYPE_SIGNATURE                       0x0
#define UAVCAN_NODE_STATUS_DATA_TYPE_SIGNATURE                          0x0f0868d0c1a7c6f1
#define UAVCAN_GET_NODE_INFO_DATA_TYPE_SIGNATURE                        0xee468a8121c46a9e
#define AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_SIGNATURE                 0xba43920c26f25016
#define AF_GET_PREFERRED_NAME_DATA_TYPE_SIGNATURE                       0xeb3f9394f779a9dd
#define AF_SET_FIELDS_DATA_TYPE_SIGNATURE                               0x7d07c058ef513a1f
#define AF_GET_FIELD_META_DATA_DATA_TYPE_SIGNATURE                      0xd997180a1e1e0e09
#define AF_TEST_BOARD_DATA_TYPE_SIGNATURE                               0x0
#define AF_PUBLISHED_FIELDS_DATA_TYPE_SIGNATURE                         0xaf951023bcd30eb0

#define UAVCAN_NODE_ID_ALLOCATION_RANDOM_TIMEOUT_RANGE_USEC             400000UL
#define UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC             600000UL

#define UNIQUE_ID_LENGTH_BYTES                                          16

#define UAVCAN_NODE_STATUS_MESSAGE_SIZE                                 7

#define UAVCAN_NODE_HEALTH_OK                                           0
#define UAVCAN_NODE_HEALTH_WARNING                                      1
#define UAVCAN_NODE_HEALTH_ERROR                                        2
#define UAVCAN_NODE_HEALTH_CRITICAL                                     3

#define UAVCAN_NODE_MODE_OPERATIONAL                                    0
#define UAVCAN_NODE_MODE_INITIALIZATION                                 1


// #define UAVCAN_GET_NODE_INFO_RESPONSE_MAX_SIZE                          ((3015 + 7) / 8)
// #define AF_SUBSCRIBE_DEFAULT_FIELDS_REQUEST_MAX_SIZE       2
// #define AF_SUBSCRIBE_DEFAULT_FIELDS_RESPONSE_MAX_SIZE      0
// #define AF_GET_PREFERRED_NAME_REQUEST_MAX_SIZE             0
// #define AF_GET_PREFERRED_NAME_RESPONSE_MAX_SIZE            256
// #define AF_SET_FIELDS_REQUEST_MAX_SIZE                     256
// #define AF_SET_FIELDS_RESPONSE_MAX_SIZE                    0
// #define AF_GET_FIELD_META_DATA_REQUEST_MAX_SIZE            0 // TODO: is this correct?
// #define AF_GET_FIELD_META_DATA_RESPONSE_MAX_SIZE           256
// #define AF_PUBLISHED_FIELDS_MAX_SIZE                       256



#define MAX_UAVCAN_TX_RX_ITERATIONS 20 // TODO: Any reason not to make this really high?

#define MAX_CAN_RX_PACKET_BYTES 256

typedef struct {
    uint16_t txUavCount;
    uint16_t rxUavCount;
    uint16_t forwardingFailedCount;
    uint16_t droppedTxPacketsOutOfMemory;
    uint16_t droppedTxPacketsOverBandwidth;
    uint16_t txFailedToTransmit;
    uint16_t rxInvalidUavFrameCount;
    uint16_t rxCanOverflowCount;
    uint64_t maxUavCanLoopDiff;
    uint16_t failedToRespond;
#ifdef H7
    FDCAN_ErrorCountersTypeDef errorCounters;
    FDCAN_ProtocolStatusTypeDef protocolStatus;
#endif
} tCanStats;

typedef struct {
	uint8_t start;
	uint8_t end;
	uint8_t size;
	uint8_t maxUsage;
	CanardCANFrame * entries;
} canBuffer;


typedef struct {
    CanardInstance gCanard; // The library instance
    uint8_t canardMemoryPool[NUM_CANARD_MEMORY_POOL_BYTES]; // Arena for memory allocation, used by the library
    // 13 bytes per CanardCANFrame (probably packed in 16 bytes)
    // CanardCANFrame is not efficiency packed - could make arrays of each element and save space
    CanardCANFrame txBufferEntries[TX_CAN_BUFFER_SIZE];
    CanardCANFrame rxBufferEntries[RX_CAN_BUFFER_SIZE];
    canBuffer txBuffer;
    canBuffer rxBuffer;
    uint8_t currentRxPacket[MAX_CAN_RX_PACKET_BYTES] __attribute__((aligned));
    // Variables used for dynamic node ID allocation.
    // RTFM at http://uavcan.org/Specification/6._Application_level_functions/#dynamic-node-id-allocation
    uint64_t sendNextNodeIdAllocationRequestAt; // When the next node ID allocation request should be sent
    uint8_t nodeIdAllocationUniqueIdOffset; // Depends on the stage of the next request
    bool newNodeAllocationInfo;
    bool nodeIdAllocated;
    // Node status variables.
    uint64_t next1hzServiceTime;
    uint8_t nodeHealth;
    uint8_t nodeMode;
    // TODO: put these back to uint32's and handle wrapping
    uint64_t subscribeBeginTick;
    uint64_t subscribeUntilTick;
    uint8_t subscribeBandwidthLimit;
    uint64_t subscribeBytesPublished;
    tCanStats stats;
    uint64_t globalMicroSeconds; // valid to the nearest 100us
} tCanDrv;

static tCanDrv canDrv = {};


// Needed by canard_stm32.c
int usleep(useconds_t usec) {
    HAL_Delay((uint32_t) (usec / 1000));
    return 0;
}

uint64_t getMonotonicTimestampUSec() {
    return canDrv.globalMicroSeconds;
}

void incrementTimestampUsec(uint32_t inc) {
    canDrv.globalMicroSeconds += inc;
}

uint64_t getDataTypeSignature(bool isBroadcast, uint16_t dataTypeId) {
    if (isBroadcast) {
        switch (dataTypeId) {
            case UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID:
                return UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_SIGNATURE;
            case UAVCAN_NODE_STATUS_DATA_TYPE_ID:
                return UAVCAN_NODE_STATUS_DATA_TYPE_SIGNATURE;
            case AF_PUBLISHED_FIELDS_DATA_TYPE_ID:
                return AF_PUBLISHED_FIELDS_DATA_TYPE_SIGNATURE;
            case AF_TEST_BOARD_RESULTS_DATA_TYPE_ID:
                return 0;
            case AF_TEST_BOARD_DATA_TYPE_ID:
                return 0;
            case AF_PROTECT_BOARD_DATA_TYPE_ID:
                return 0;
            default:
                softAssert(0, "Unknown dataTypeId");
                return 0;
        }
    } else {
        switch (dataTypeId) {
            case UAVCAN_GET_NODE_INFO_DATA_TYPE_ID:
                return UAVCAN_GET_NODE_INFO_DATA_TYPE_SIGNATURE;
            case AF_GET_PREFERRED_NAME_DATA_TYPE_ID:
                return AF_GET_PREFERRED_NAME_DATA_TYPE_SIGNATURE;
            case AF_GET_FIELD_META_DATA_DATA_TYPE_ID:
                return AF_GET_FIELD_META_DATA_DATA_TYPE_SIGNATURE;
            case AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_ID:
                return AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_SIGNATURE;
            case AF_SET_FIELDS_DATA_TYPE_ID:
                return AF_SET_FIELDS_DATA_TYPE_SIGNATURE;
            default:
                softAssert(0, "Unknown dataTypeId");
                return 0;
        }
    }
}


// ============================= //

// TODO: Try and reduce this down



static int16_t canLocalOrRemoteBroadcast(bool canOverSerial,
                    uint8_t sourceNodeId, // Allows a different source node id for forwarding to/from serial link
                    uint16_t dataTypeId,
                    uint8_t* inoutTransferId,
                    uint8_t priority,
                    const void* payload,
                    uint16_t payload_len) {
    int16_t result;
    if (canOverSerial && USING_SERIAL_LINK) {
        result = canBroadcastOverSerial(
            sourceNodeId,
            dataTypeId,
            priority,
            inoutTransferId,
            payload,
            payload_len
        );
    } else {
        uint64_t signature = getDataTypeSignature(true, dataTypeId);
        result = canardBroadcast(
            &canDrv.gCanard,
            sourceNodeId,
            signature,
            dataTypeId,
            inoutTransferId,
            priority,
            payload,
            payload_len
        );
        if (result <= 0) {
            canDrv.stats.droppedTxPacketsOutOfMemory++;
            // softAssert(broadcast_result != -CANARD_ERROR_OUT_OF_MEMORY, "Tx can out of memory");
        }
    }
    return result;
}

static int16_t canLocalOrRemoteRequestOrRespond(bool canOverSerial,
                                                uint8_t sourceNodeId,  // Allows a different source node id for forwarding to/from serial link
                                                uint8_t destNodeId,
                                                uint16_t dataTypeId,
                                                uint8_t transferId,
                                                uint8_t priority,
                                                bool isRequest,
                                                const void* payload,
                                                uint16_t payloadLength) {
    if (canOverSerial && USING_SERIAL_LINK) {
        return canRequestResponseOverSerial(
            sourceNodeId,
            destNodeId,
            isRequest,
            dataTypeId,
            priority,
            &transferId,
            payload,
            payloadLength);
    } else {
        // Respond to the requestTransfer
        return canardRequestOrRespond(
            &canDrv.gCanard,
            sourceNodeId,
            destNodeId,
            getDataTypeSignature(false, dataTypeId),
            dataTypeId,
            &transferId,
            priority,
            isRequest ? CanardRequest : CanardResponse,
            payload,
            payloadLength);
    }
}

// ================================= //

static int16_t canBroadcast(bool canOverSerial,
                    uint16_t dataTypeId,
                    uint8_t* inoutTransferId,
                    uint8_t priority,
                    const void* payload,
                    uint16_t payloadLength) {
    uint8_t nodeId = getLocalNodeId();
    softAssert(nodeId, "The node ID needs to be set before canBroadcast");
    return canLocalOrRemoteBroadcast(
        canOverSerial,
		nodeId,
        dataTypeId,
        inoutTransferId,
        priority,
        payload,
        payloadLength
    );
}

static int16_t anonymousCanBroadcast(bool canOverSerial,
                            uint16_t dataTypeId,
                            uint8_t* inoutTransferId,
                            uint8_t priority,
                            const void* payload,
                            uint16_t payloadLength) {
    softAssert(dataTypeId < 4, "Invalid dataTypeId for anonymous broadcast");
    softAssert(getLocalNodeId() == 0, "Shouldn't be anon broadcasting when ID allocated");
    return canLocalOrRemoteBroadcast(
        canOverSerial,
        0,
        dataTypeId,
        inoutTransferId,
        priority,
        payload,
        payloadLength
    );
}

static int16_t canRequest(bool canOverSerial,
                    uint8_t destNodeId,
                    uint16_t dataTypeId,
                    uint8_t transferId,
                    uint8_t priority,
                    const void* payload,
                    uint16_t payloadLength) {
    return canLocalOrRemoteRequestOrRespond(
        canOverSerial,
        getLocalNodeId(),
        destNodeId,
        dataTypeId,
        transferId,
        priority,
        true,
        payload,
        payloadLength
    );
}

static int16_t canRespondToRequest(bool canOverSerial, CanardRxTransfer *requestTransfer, uint8_t dataTypeId, const void* payload, uint16_t payloadLength) {
    // Respond to the requestTransfer
    uint8_t destNodeId = requestTransfer->source_node_id; // Reply to source node
    return canLocalOrRemoteRequestOrRespond(
        canOverSerial,
        getLocalNodeId(),
        destNodeId,
        dataTypeId,
        requestTransfer->transfer_id,
        requestTransfer->priority,
        false,
        payload,
        payloadLength
    );
}


// ============================= //
// Node ID allocation

uint8_t getLocalNodeId() {
    return canardGetLocalNodeID(&canDrv.gCanard);
}

bool isNodeAllocated() {
    return canDrv.nodeIdAllocated;
}

static void readUniqueID(uint8_t *out_uid) {
    softAssert(12 <= UNIQUE_ID_LENGTH_BYTES, "Inv UUID size");
    uint32_t tmp[4] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2(), 0};
    for (uint8_t i = 0; i < 4; i++) {
        memcpy(&out_uid[4 * i], &tmp[i], 4);
    }
}

static void handleNodeAllocationBroadcast(CanardRxTransfer *transfer, uint8_t * data, uint8_t dataLength) {
    // Rule C - updating the randomized time interval
    uint64_t tmp = getRandomInt(UAVCAN_NODE_ID_ALLOCATION_RANDOM_TIMEOUT_RANGE_USEC);
    canDrv.sendNextNodeIdAllocationRequestAt = getMonotonicTimestampUSec()
        + UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC
        + (uint64_t) (tmp);

    // A response from another node
    if (transfer->source_node_id == CANARD_BROADCAST_NODE_ID) {
        canDrv.nodeIdAllocationUniqueIdOffset = 0;
        return;
    }

    // Obtaining the local unique ID
    uint8_t myId[UNIQUE_ID_LENGTH_BYTES];
    readUniqueID(myId);

    // Copying the unique ID from the message
    uint8_t receivedIdLen = dataLength - 1;
    uint8_t * receivedId = &data[1];

    // Matching the received UID against the local one
    if (memcmp(receivedId, myId, receivedIdLen) != 0) {
        canDrv.nodeIdAllocationUniqueIdOffset = 0;
        return; // No match, return
    }
    canDrv.newNodeAllocationInfo = 1;

    if (receivedIdLen < UNIQUE_ID_LENGTH_BYTES) {
        // The allocator has confirmed part of unique ID, switching to the next stage and updating the timeout.
        canDrv.nodeIdAllocationUniqueIdOffset = receivedIdLen;
    } else {
        // Allocation complete - copying the allocated node ID from the message
        uint8_t allocated_node_id = data[0] >> 1;
        if (allocated_node_id > 127) {
            canDrv.nodeIdAllocationUniqueIdOffset = 0;
            softAssert(0, "Inv node id");
        } else {
            canardSetLocalNodeID(&canDrv.gCanard, allocated_node_id);
            canDrv.nodeIdAllocated = 1;
        }
    }
}

static void sendNodeAllocationFrame() {
    // Structure of the request is documented in the DSDL definition
    // See http://uavcan.org/Specification/6._Application_level_functions/#dynamic-node-id-allocation
    const uint8_t PreferredNodeID = CANARD_BROADCAST_NODE_ID; ///< This can be made configurable, obviously
    uint8_t allocationRequest[CANARD_CAN_FRAME_MAX_DATA_LEN - 1];
    allocationRequest[0] = (uint8_t) (PreferredNodeID << 1U);

    if (canDrv.nodeIdAllocationUniqueIdOffset == 0) {
        allocationRequest[0] |= 1; // First part of unique ID
    }

    uint8_t myId[UNIQUE_ID_LENGTH_BYTES];
    readUniqueID(myId);

    static const uint8_t maxLenOfUniqueIDInRequest = 6;
    uint8_t uid_size = (uint8_t) (UNIQUE_ID_LENGTH_BYTES - canDrv.nodeIdAllocationUniqueIdOffset);
    if (uid_size > maxLenOfUniqueIDInRequest) {
        uid_size = maxLenOfUniqueIDInRequest;
    }

    // Paranoia time
    hardAssert(canDrv.nodeIdAllocationUniqueIdOffset < UNIQUE_ID_LENGTH_BYTES, "Inv uid offset");
    hardAssert(uid_size <= maxLenOfUniqueIDInRequest, "Inv uid_size");
    hardAssert(uid_size > 0, "Inv uid_size");
    hardAssert((uid_size + canDrv.nodeIdAllocationUniqueIdOffset) <= UNIQUE_ID_LENGTH_BYTES, "Inv uid_size");

    memmove(&allocationRequest[1], &myId[canDrv.nodeIdAllocationUniqueIdOffset], uid_size);

    // Broadcasting the request for node ID
    // On both the can and serial side
    static uint8_t transferId;
    anonymousCanBroadcast(false, UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID, &transferId, CANARD_TRANSFER_PRIORITY_LOW, &allocationRequest[0], (uint16_t) (uid_size + 1));
    anonymousCanBroadcast(true,  UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID, &transferId, CANARD_TRANSFER_PRIORITY_LOW, &allocationRequest[0], (uint16_t) (uid_size + 1));

    // Reset state - it will be filled in if there is a response
    canDrv.nodeIdAllocationUniqueIdOffset = 0;
}

static void loopNodeAllocation() {
    // Performing the dynamic node ID allocation procedure.
    static uint64_t timeoutTime = 0;

    // Everytime there is a response send the next part of the uuid
    // Everytime it times out send the beginning of the UUID
    if ((getMonotonicTimestampUSec() > timeoutTime) || canDrv.newNodeAllocationInfo) {
        canDrv.newNodeAllocationInfo = 0;
        timeoutTime = getMonotonicTimestampUSec()
            + UAVCAN_NODE_ID_ALLOCATION_REQUEST_DELAY_OFFSET_USEC
            + (uint64_t) (getRandomInt(UAVCAN_NODE_ID_ALLOCATION_RANDOM_TIMEOUT_RANGE_USEC));
        sendNodeAllocationFrame();
    }
}

// ============================ //
// Node status

static void makeNodeStatusMessage(uint8_t buffer[UAVCAN_NODE_STATUS_MESSAGE_SIZE]) {
    memset(buffer, 0, UAVCAN_NODE_STATUS_MESSAGE_SIZE);

    static uint32_t startedAtSec = 0;
    if (startedAtSec == 0) {
        startedAtSec = (uint32_t) (getMonotonicTimestampUSec() / 1000000U);
    }
    const uint32_t uptimeSec = ((uint32_t) (getMonotonicTimestampUSec() / 1000000U)) - startedAtSec;

    canardEncodeScalar(buffer, 0, 32, &uptimeSec);
    canardEncodeScalar(buffer, 32, 2, &canDrv.nodeHealth);
    canardEncodeScalar(buffer, 34, 3, &canDrv.nodeMode);
}

static void getNodeInfoResponse(uint8_t * buffer, uint32_t * bufferSize, uint32_t maxBufferSize) {
    // NodeStatus
    makeNodeStatusMessage(buffer);

    // SoftwareVersion
    buffer[7] = APP_VERSION_MAJOR;
    buffer[8] = APP_VERSION_MINOR;
    buffer[9] = 1; // Optional field flags, VCS commit is set
    uint32_t u32 = GIT_HASH;
    canardEncodeScalar(buffer, 80, 32, &u32);
    // Image CRC skipped

    // HardwareVersion
    // Major skipped
    // Minor skipped
    readUniqueID(&buffer[24]);
    // Certificate of authenticity skipped

    // Name
    const size_t nameLen = strlen(getBoardNodeName());
    *bufferSize = 41 + nameLen;
    softAssert(*bufferSize < maxBufferSize, "Exceeded node info max size");
    memcpy(&buffer[41], getBoardNodeName(), nameLen);
}

// ============================ //

static bool canShouldAcceptTransfer(CanardTransferType transferType, uint16_t dataTypeId, uint64_t *outDataTypeSignature) {
    *outDataTypeSignature = getDataTypeSignature((transferType == CanardTransferTypeBroadcast), dataTypeId);
    if (transferType == CanardTransferTypeBroadcast) {
        if ((getLocalNodeId() == CANARD_BROADCAST_NODE_ID) && (dataTypeId == UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID)) {
            return true;
        }
        if (dataTypeId == AF_TEST_BOARD_DATA_TYPE_ID) {
            return true;
        }
        if (dataTypeId == AF_PROTECT_BOARD_DATA_TYPE_ID) {
            return true;
        }
        
        // if (dataTypeId == AF_TEST_BOARD_RESULTS_DATA_TYPE_ID) {
        //     return true;
        // }
    } else {
        if (dataTypeId == AF_SET_FIELDS_DATA_TYPE_ID) {
            return true;
        } else if (dataTypeId == UAVCAN_GET_NODE_INFO_DATA_TYPE_ID) {
            return true;
        } else if (dataTypeId == AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_ID) {
            return true;
        } else if (dataTypeId == AF_GET_FIELD_META_DATA_DATA_TYPE_ID) {
            return true;
        } else if (dataTypeId == AF_GET_PREFERRED_NAME_DATA_TYPE_ID) {
            return true;
        }
    }
    return false;
}

static void canProcessRxFrame(bool canOverSerial, CanardRxTransfer *transfer, uint8_t destNodeId, uint8_t * data, uint8_t dataLength) {
    if (transfer->transfer_type == CanardTransferTypeRequest && destNodeId == getLocalNodeId()) {
        int16_t result = 0;
        
        if (transfer->data_type_id == AF_SET_FIELDS_DATA_TYPE_ID) {
            afProtocolProcessRx(data, dataLength);
            result = canRespondToRequest(canOverSerial, transfer, AF_SET_FIELDS_DATA_TYPE_ID, NULL, 0);

        } else if (transfer->data_type_id == UAVCAN_GET_NODE_INFO_DATA_TYPE_ID) {
            #define NODE_INFO_RESPONSE_MAX_SIZE 100
            uint8_t buffer[NODE_INFO_RESPONSE_MAX_SIZE];
            memset(buffer, 0, NODE_INFO_RESPONSE_MAX_SIZE);
            uint32_t totalSize;
            getNodeInfoResponse(buffer, &totalSize, NODE_INFO_RESPONSE_MAX_SIZE);
            // Transmitting; in this case we don't have to release the payload because it's empty anyway.
            result = canRespondToRequest(canOverSerial, transfer, UAVCAN_GET_NODE_INFO_DATA_TYPE_ID, &buffer[0], (uint16_t) totalSize);
            
        } else if (transfer->data_type_id == AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_ID) {
            uint32_t tick = HAL_GetTick();
            uint8_t seconds = data[0];
            uint8_t bandwidth = data[1];
            canDrv.subscribeBeginTick = tick;
            canDrv.subscribeUntilTick = tick + 1000 * (uint32_t) seconds;
            canDrv.subscribeBandwidthLimit = bandwidth;
            canDrv.subscribeBytesPublished = 0; // Reset the counter when the subscription is renewed
            result = canRespondToRequest(canOverSerial, transfer, AF_SUBSCRIBE_DEFAULT_FIELDS_DATA_TYPE_ID, NULL, 0);
            
        } else if (transfer->data_type_id == AF_GET_PREFERRED_NAME_DATA_TYPE_ID) {
            const char *name = getBoardName(); // TODO: Get (and set) from flash
            result = canRespondToRequest(canOverSerial, transfer, AF_GET_PREFERRED_NAME_DATA_TYPE_ID, name, strlen(name));
            
        } else if (transfer->data_type_id == AF_GET_FIELD_META_DATA_DATA_TYPE_ID) {
            uint8_t * responsePacket = NULL;
            uint16_t responseLength;
            handleMetaDataRequest(data, dataLength, &responsePacket, &responseLength);
            hardAssert(responsePacket, "");
            result = canRespondToRequest(canOverSerial, transfer, AF_GET_FIELD_META_DATA_DATA_TYPE_ID, responsePacket, responseLength);

        }

        if (result <= 0) {
            // softAssert(0, "Failed to respond");
            canDrv.stats.failedToRespond++;
        }

    } else if (transfer->transfer_type == CanardTransferTypeBroadcast) {
        /*
        * Dynamic node ID allocation protocol.
        * Taking this branch only if we don't have a node ID,	 ignoring otherwise.
        */
        if ((getLocalNodeId() == CANARD_BROADCAST_NODE_ID)
            && (transfer->data_type_id == UAVCAN_NODE_ID_ALLOCATION_DATA_TYPE_ID)) {
            handleNodeAllocationBroadcast(transfer, data, dataLength);

        } else if (transfer->data_type_id == AF_TEST_BOARD_DATA_TYPE_ID) {
            // Use this extraId to match up the response to the request
            // in case a board is unplugged and a new one is plugged in whilst the frames are in flight
            uint8_t extraId = data[0];
            // Do the actual self test
            uint8_t errorCode = boardTest();
            // Check that the board is protected - we don't want to send out boards that are not protected
            uint32_t rdpLevel = readRdpValue();
            if (rdpLevel != OB_RDP_LEVEL_2) {
                errorCode |= 0x80;
            }
            uint8_t packet[2] = {errorCode, extraId};
            static uint8_t transferId;
            anonymousCanBroadcast(canOverSerial, AF_TEST_BOARD_RESULTS_DATA_TYPE_ID, &transferId, CANARD_TRANSFER_PRIORITY_LOW, packet, 2);
            
        } else if (transfer->data_type_id == AF_PROTECT_BOARD_DATA_TYPE_ID) {
            uint8_t level = data[0];
            setRdp(level);
            // uint8_t packet[1] = {0};
            // static uint8_t transferId;
            // anonymousCanBroadcast(canOverSerial, AF_TEST_BOARD_RESULTS_DATA_TYPE_ID, &transferId, CANARD_TRANSFER_PRIORITY_LOW, packet, 1);
        }
    }
}

void canOnTransferReceived(bool canOverSerial, CanardRxTransfer * transfer, uint8_t destNodeId, const void* payload, uint16_t payloadLength) {
    bool isBroadcast = (transfer->transfer_type == CanardTransferTypeBroadcast);
    
    // Process any frames for this node
    if (isBroadcast || (destNodeId == getLocalNodeId())) {
        canProcessRxFrame(canOverSerial, transfer, destNodeId, (void*) payload, payloadLength);
    }

    // If serial link then forward all frames not specifically for this node
    if (USING_SERIAL_LINK) {
        int16_t result = 0;
        // Forward all broadcasts
        if (isBroadcast) {
            result = canLocalOrRemoteBroadcast(
                                !canOverSerial, // send on the opposite interface it came in on
                                transfer->source_node_id,
                                transfer->data_type_id,
                                &transfer->transfer_id,
                                transfer->priority,
                                payload,
                                payloadLength);
        
        // Forward all packets for other nodes
        } else if (destNodeId != getLocalNodeId()) {
            result = canLocalOrRemoteRequestOrRespond(
                                !canOverSerial, // send on the opposite interface it came in on
                                transfer->source_node_id,
                                destNodeId,
                                transfer->data_type_id,
                                transfer->transfer_id,
                                transfer->priority,
                                (transfer->transfer_type == CanardTransferTypeRequest),
                                payload,
                                payloadLength);
        }
        if (result < 0) {
            softAssert(0, "Failed to forward can packet");
            canDrv.stats.forwardingFailedCount++;
        }
    }
}

// Call the canardDecodeScalar to get the data
// Assumes the maximum payload is 255 but UAVCAN go much higher than this
static uint8_t * convertTransferToDataPtr(CanardRxTransfer * transfer, uint8_t * length) {
    if (transfer->payload_len >= 256) {
        softAssert(0, "Exceeded expected packet length");
        *length = 0;
        return NULL;
    } else {
        *length = transfer->payload_len;
        uint8_t bytesRead = 0;
        while (bytesRead < *length) {
            uint16_t readLength = MIN((*length - bytesRead), 8);
            canardDecodeScalar(transfer, 8*bytesRead, 8*readLength, false, &canDrv.currentRxPacket[bytesRead]);
            bytesRead += readLength;
        }
        return canDrv.currentRxPacket;
    }
}

static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer, uint8_t destNodeId) {
    uint8_t length;
    uint8_t * data = convertTransferToDataPtr(transfer, &length);
    if (data == NULL)
        return;
    canOnTransferReceived(false, transfer, destNodeId, data, length);
}

static bool shouldAcceptTransfer(const CanardInstance *ins, uint64_t *outDataTypeSignature, uint16_t dataTypeId, CanardTransferType transferType, uint8_t sourceNodeId) {
    (void) sourceNodeId;
    return canShouldAcceptTransfer(transferType, dataTypeId, outDataTypeSignature);
}

// ============================= //

#ifdef H7

int16_t canardFDTransmit(const CanardCANFrame * txFrame) {
    FDCAN_TxHeaderTypeDef header;
    header.IdType = FDCAN_EXTENDED_ID;
    header.TxFrameType = FDCAN_DATA_FRAME; //FDCAN_DATA_FRAME, FDCAN_REMOTE_FRAME
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE; // FDCAN_ESI_ACTIVE, FDCAN_ESI_PASSIVE
    header.BitRateSwitch = FDCAN_BRS_OFF;
    header.FDFormat = FDCAN_CLASSIC_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS; // FDCAN_STORE_TX_EVENTS
    header.MessageMarker = 0; //??
    
    softAssert(txFrame->data_len <= 8, "Check out FDCAN_data_length_code for lengths > 8");
    header.DataLength = txFrame->data_len << 16;
    header.Identifier = txFrame->id & (~CANARD_CAN_FRAME_EFF);
    HAL_StatusTypeDef res = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &header, (uint8_t *) txFrame->data);
    if (res == HAL_OK) {
        return 1;
    } else {
        if (hfdcan1.ErrorCode == HAL_FDCAN_ERROR_FIFO_FULL) {
            canDrv.stats.droppedTxPacketsOutOfMemory++;
        } else {
            softAssert(0, "Failed to transmit CAN frame");
        }
        return -1;
    }
}

int16_t canardFDReceive(CanardCANFrame * rxFrame) {
    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) == 0) {
        return 0;
    }
    FDCAN_RxHeaderTypeDef header;
    HAL_StatusTypeDef res = HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &header, &rxFrame->data[0]);
    rxFrame->id = header.Identifier;
    if (FDCAN_EXTENDED_ID == header.IdType) {
        rxFrame->id |= CANARD_CAN_FRAME_EFF;
    }
    rxFrame->data_len = 0xFF & (header.DataLength >> 16);
    if (res == HAL_OK) {
        return 1;
    } else {
        softAssert(0, "Failed to receive CAN frame");
        return -1;
    }
}

#endif


void bufferRawCanFrames(const uint64_t timestamp) {
    // Buffer multiple Rx frames
    for (uint32_t r=10; r--; ) {
        CanardCANFrame rxFrame;
        // TODO: remove ifdefs
        #ifdef H7
            const int16_t rxResult = canardFDReceive(&rxFrame);
        #else
            const int16_t rxResult = canardSTM32Receive(&rxFrame);
        #endif
        if (rxResult == 0) {
            break;
        } else if (rxResult > 0) { // Success
            if (BUFFER_FULL(canDrv.rxBuffer)) {
                canDrv.stats.rxCanOverflowCount++;
            } else {
                BUFFER_APPEND(canDrv.rxBuffer, rxFrame);
                canDrv.rxBuffer.maxUsage = MAX(BUFFER_LENGTH(canDrv.rxBuffer), canDrv.rxBuffer.maxUsage);
            }
        } else {
            softAssert(0, "Inv arg"); // Shouldn't happen
        }
    }

    // Send 1 tx frame
    if (!BUFFER_EMPTY(canDrv.txBuffer)) {
        const CanardCANFrame * txFrame = BUFFER_PEEK(canDrv.txBuffer);
        // softAssert(canardSTM32GetStats().error_count == 0, "TX error");
        // TODO: remove ifdefs
        #ifdef H7
            const int16_t txResult = canardFDTransmit(txFrame);
        #else
            const int16_t txResult = canardSTM32Transmit(txFrame);
        #endif
        if (txResult > 0) { // Success
            BUFFER_POP(canDrv.txBuffer);
        } else if (txResult < 0) { // Failure
            canDrv.stats.txFailedToTransmit++;
            BUFFER_POP(canDrv.txBuffer);  // Note: We don't retry on error
        } else { // Timeout
            // break; // Note: We do retry on timeout (mailbox busy)
        }
    }
}

static void processUavCanFrames(const uint64_t timestamp) {
    // Check that time passed is not too much
    //static uint64_t nextTime = -1;
    //softAssert(timestamp < nextTime, "Too long between processUavCanFrames calls");
    // For a CAN data rate of 1Mbps every bit of memory corresponds 1us
    // The canard library won't be 100% efficient but then the headers of each packet
    // won't be stored
    //nextTime = timestamp + (NUM_CANARD_MEMORY_POOL_BYTES * 8);

    bool txValid = 1;
    bool rxValid = 1;

    for (uint8_t iter=0; (iter<MAX_UAVCAN_TX_RX_ITERATIONS) && (txValid || rxValid); iter++) {
        // Buffer tx frame
        const CanardCANFrame * txFrame = canardPeekTxQueue(&canDrv.gCanard);
        txValid = (txFrame != NULL && !BUFFER_FULL(canDrv.txBuffer));
        if (txValid) {
            canDrv.stats.txUavCount++;
            BUFFER_APPEND(canDrv.txBuffer, *txFrame);
            canDrv.txBuffer.maxUsage = MAX(BUFFER_LENGTH(canDrv.txBuffer), canDrv.txBuffer.maxUsage);
            hookCanFrameTransmitted(timestamp, txFrame);
            canardPopTxQueue(&canDrv.gCanard);
        }
        // Process Rx frame
        rxValid = !BUFFER_EMPTY(canDrv.rxBuffer);
        if (rxValid) {
            canDrv.stats.rxUavCount++;
            CanardCANFrame * rxFrame = BUFFER_PEEK(canDrv.rxBuffer);
            hookCanFrameReceived(timestamp, rxFrame);
            volatile int16_t res = canardHandleRxFrame(&canDrv.gCanard, rxFrame, timestamp, USING_SERIAL_LINK);
            if (!(res == 0
                || res == -CANARD_ERROR_RX_NOT_WANTED
                || res == -CANARD_ERROR_RX_WRONG_ADDRESS)) {
                // softAssert(0, "Bad rx frame");
                canDrv.stats.rxInvalidUavFrameCount++;
            }
            BUFFER_POP(canDrv.rxBuffer);
        }
    }
}

// This function is called at 1 Hz rate from the main loop.
static void process1HzTasks(uint64_t timestamp_usec) {
    // Purging transfers that are no longer transmitted. This will occasionally free up some memory.
    canardCleanupStaleTransfers(&canDrv.gCanard, timestamp_usec);
    // Transmitting the node status message periodically.
    if (canDrv.nodeIdAllocated) {
        uint8_t buffer[UAVCAN_NODE_STATUS_MESSAGE_SIZE];
        makeNodeStatusMessage(buffer);
        static uint8_t transfer_id; // Note that the transfer ID variable MUST BE STATIC (or heap-allocated)!
        canBroadcast(true,  UAVCAN_NODE_STATUS_DATA_TYPE_ID, &transfer_id, CANARD_TRANSFER_PRIORITY_LOW, buffer, UAVCAN_NODE_STATUS_MESSAGE_SIZE);
        canBroadcast(false, UAVCAN_NODE_STATUS_DATA_TYPE_ID, &transfer_id, CANARD_TRANSFER_PRIORITY_LOW, buffer, UAVCAN_NODE_STATUS_MESSAGE_SIZE);
        //softAssert(bc_res > 0, "Failed to broadcast node status");
    }

    canDrv.nodeMode = UAVCAN_NODE_MODE_OPERATIONAL;
}

static void record1HzStats() {
    // Check that whether the underlying rx fifo has overflowed
    // TODO: fix
#ifdef H7

    softAssert(HAL_FDCAN_GetErrorCounters(&hfdcan1, &canDrv.stats.errorCounters) == HAL_OK, "HAL_FDCAN_GetErrorCounters failed");
    softAssert(HAL_FDCAN_GetProtocolStatus(&hfdcan1, &canDrv.stats.protocolStatus) == HAL_OK, "HAL_FDCAN_GetErrorCounters failed");

#else
    static uint32_t lastRxOverflowCount = 0;
    static uint32_t lastRxTidErrorCount = 0;
	if (canardSTM32GetStats().rx_overflow_count != lastRxOverflowCount) {
		// softAssert(0, "RX overflowed");
		canDrv.stats.rxCanOverflowCount++;
		lastRxOverflowCount = canardSTM32GetStats().rx_overflow_count;
	}
    if (canDrv.gCanard.rx_tid_errors != lastRxTidErrorCount) {
        // softAssert(0, "RX error");
        canDrv.stats.rxInvalidUavFrameCount++;
        lastRxTidErrorCount = canDrv.gCanard.rx_tid_errors;
    }
#endif

}

// ================= //


// Takes ~7us to run when no frames - called every 100us
void loopRawCan() {
    // General timer - as this is called more often than ticks
    canDrv.globalMicroSeconds += 100; // This should be called by a timer every 100us
    bufferRawCanFrames(canDrv.globalMicroSeconds);
}

void loopUavCan() {
    const uint64_t timestamp = getMonotonicTimestampUSec();
    // TODO: check that time passed is not too much
    canDrv.stats.maxUavCanLoopDiff = MAX(canDrv.stats.maxUavCanLoopDiff, (getMonotonicTimestampUSec() - timestamp));

    processUavCanFrames(timestamp);

    if (!canDrv.nodeIdAllocated) {
        loopNodeAllocation();
    }

    // General cleanup
    if (timestamp >= canDrv.next1hzServiceTime) {
        canDrv.next1hzServiceTime = timestamp + 1000000;
        record1HzStats();
        process1HzTasks(timestamp);
    }
}

void setupCan() {
    canDrv.nodeHealth = UAVCAN_NODE_HEALTH_OK;
    canDrv.nodeMode = UAVCAN_NODE_MODE_INITIALIZATION;
    canDrv.txBuffer.size = TX_CAN_BUFFER_SIZE;
    canDrv.txBuffer.entries = canDrv.txBufferEntries;
    canDrv.rxBuffer.size = RX_CAN_BUFFER_SIZE;
    canDrv.rxBuffer.entries = canDrv.rxBufferEntries;

    // UAVCAN driver obtained from https://github.com/UAVCAN/libcanard
    // Tutorial: https://kb.zubax.com/display/MAINKB/1.+Basic+tutorial

    // STM32 peripheral configuration for CAN has been performed, but
    // calling of MX_CAN_Init is inhibited within the .iod file using
    // Project Manager / Advanced Settings, as libcanard configures
    // the bxCAN interface itself. However we still need to call the
    // low-level initialisation routine HAL_CAN_MspInit to set pin-out.

    // TODO: remove ifdefs
    #ifdef H7
        softAssert((HAL_FDCAN_Start(&hfdcan1) == HAL_OK), "CAN start failed");
    #else
        CanardSTM32CANTimings timings;
        int result = canardSTM32ComputeCANTimings(HAL_RCC_GetPCLK1Freq(), 1000000, &timings);
        hardAssert(result == 0, "ComputeCANTimings failed");
        result = canardSTM32Init(&timings, CanardSTM32IfaceModeNormal);
        hardAssert(result == 0, "canardSTM32Init failed");
    #endif

    canDrv.next1hzServiceTime = 0;
    canardInit(&canDrv.gCanard, // Uninitialized library instance
        canDrv.canardMemoryPool, // Raw memory chunk used for dynamic allocation
        sizeof(canDrv.canardMemoryPool), // Size of the above, in bytes
        onTransferReceived, // Callback, see CanardOnTransferReception
        shouldAcceptTransfer, // Callback, see CanardShouldAcceptTransfer
        NULL);
    
    canDrv.globalMicroSeconds = 1000 * (uint64_t) HAL_GetTick();
}

// ================= //

void requestSetFields(uint8_t destNodeId, uint8_t packetBytes[], size_t numBytes) {
    if (!canDrv.nodeIdAllocated)
        return;
    if (numBytes >= 256) {
        softAssert(0, "Packet too big");
        return;
    }
    static uint8_t transferId; // TODO - needs to be per data type??
    canRequest(
        USING_SERIAL_LINK,
        destNodeId,
        AF_SET_FIELDS_DATA_TYPE_ID,
        transferId,
        CANARD_TRANSFER_PRIORITY_MEDIUM,
        packetBytes,
        numBytes
    );
}

void broadcastFieldsPacket(uint8_t packetBytes[], size_t numBytes) {
    if (!canDrv.nodeIdAllocated)
        return;
    if (numBytes >= 256) {
        softAssert(0, "Packet too big");
        return;
    }
    static uint8_t transferId;
    const int16_t broadcast_result = canBroadcast(
        USING_SERIAL_LINK, // If serial protocol enabled only send field data on serial
        AF_PUBLISHED_FIELDS_DATA_TYPE_ID,
        &transferId,
        CANARD_TRANSFER_PRIORITY_MEDIUM,
        (const void*) packetBytes,
        numBytes
    );
    if (broadcast_result >= 1) {
        canDrv.subscribeBytesPublished += numBytes;
    }
}

int8_t publishFieldPacketIfBelowBandwidth(uint8_t packetBytes[], size_t numBytes) {
    uint32_t tick = HAL_GetTick();
    uint32_t bandwidth = canDrv.subscribeBytesPublished / (tick - canDrv.subscribeBeginTick); // TODO: try and avoid divides
    if (tick >= canDrv.subscribeBeginTick && tick <= canDrv.subscribeUntilTick) {
        if (bandwidth < (uint32_t) canDrv.subscribeBandwidthLimit) {
            broadcastFieldsPacket(packetBytes, numBytes);
            return 0;
        } else {
            canDrv.stats.droppedTxPacketsOverBandwidth++;
            return -1;
        }
    } else {
        // Not subscribed
        // canDrv.stats.not++;
        return -1;
    }
}

bool allCanFramesSent() {
    const CanardCANFrame* frame = canardPeekTxQueue(&canDrv.gCanard);
    return ((frame == NULL) && BUFFER_EMPTY(canDrv.txBuffer));
}

bool allCanFramesReceived() {
    return (BUFFER_EMPTY(canDrv.rxBuffer));
}


// ========================= //
// Testing

void testSetNodeId(uint8_t nodeId) {
    canDrv.nodeIdAllocated = true;
    canardSetLocalNodeID(&canDrv.gCanard, nodeId);
}

void testLoopbackCan() {
    while (!BUFFER_FULL(canDrv.rxBuffer) && !BUFFER_EMPTY(canDrv.txBuffer)) {
        const CanardCANFrame * frame = BUFFER_PEEK(canDrv.txBuffer);
        BUFFER_APPEND(canDrv.rxBuffer, *frame);
        BUFFER_POP(canDrv.txBuffer);
    }
}

uint16_t testUavRxCount() {
    return canDrv.stats.rxUavCount;
}
uint16_t testUavTxCount() {
    return canDrv.stats.txUavCount;
}


