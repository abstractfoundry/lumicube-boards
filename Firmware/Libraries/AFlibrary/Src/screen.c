/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_SCREEN_MODULE


#include <stdint.h>
#include <string.h>
#include "afException.h"
#include "useful.h"
#include "afFieldProtocol.h"
#include "tft.h"
#include "fonts.h"

#ifndef SCREEN_FIELDS_OFFSET
    #define SCREEN_FIELDS_OFFSET 0
#endif

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define DMA_BUFFER_SIZE_BYTES (SCREEN_WIDTH*2) // DO NOT CHANGE: it's the size of a row

#define MAX_PACKET_PIXELS 80 // 80 * 2B -> 160B (which fits into the max packet size ~250B)
#define NUM_CHARACTERS 128
#define COMMAND_BUFFER_LENGTH 255 // 32 bit words
#define DATA_BUFFER_BYTES 512

typedef struct {
    uint8_t command;
    uint8_t length;
    uint16_t dataBufferStart;
} tCommandWord;

typedef struct {
	uint8_t start;
	uint8_t end;
	uint8_t size;
	tCommandWord entries[COMMAND_BUFFER_LENGTH];
} tCommandBuffer;


typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} tPixelWindow;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t colour;
} tDrawRectangle;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t size;
    uint16_t colour;
    uint16_t backgroundColour;
    uint16_t numChar;
} tDrawText;

typedef struct {
    uint16_t running;
    uint16_t rowPixelIndex;
} tPixelStream;

typedef struct {
    bool changeFreq;
    GPIO_TypeDef* backlightGpioSection;
    uint16_t backlightGpioPin;
    uint32_t cmdSpaceRemaining;
    uint8_t backlightEnabled;
    uint8_t invertColours;
    uint8_t rotation;
    uint8_t resolutionScaling;
    uint8_t newResolutionScaling;

    tDrawRectangle drawRectangle;
    char text[NUM_CHARACTERS];
    tDrawText drawText;

    // uint16_t triangleX1;
    // uint16_t triangleY1;
    // uint16_t triangleX2;
    // uint16_t triangleY2;
    // uint16_t triangleX3;
    // uint16_t triangleY3;
    // uint16_t triangleColour;

    tPixelWindow pixelWindow;
    tPixelWindow newPixelWindow;
    tPixelStream pixelStream;

    // Store all incoming commands because they can take a while to complete
    uint8_t dataBufferData[DATA_BUFFER_BYTES];
    sCircularBuffer dataBuffer;

    tCommandBuffer commandBuffer;

    // DMA buffer - needs to be big enough to fit an entire row
} tScreenBoardDriver;

tScreenBoardDriver screen = {};

__attribute__ ((section(".dmadata")))
uint8_t screenDmaBuffer[DMA_BUFFER_SIZE_BYTES];

#define SET_PIXEL_WINDOW_COMMAND 0x1
#define START_STREAM_PIXELS_COMMAND 0x2
#define STREAM_PIXELS_COMMAND 0x3
#define DRAW_RECTANGLE_COMMAND 0x4
#define DRAW_TEXT_COMMAND 0x5
#define CHANGE_RESOLUTION_SCALING 0x6
#define STOP_STREAM_PIXELS_COMMAND 0x7

// ============================================ //

void drawText(uint16_t startX, uint16_t startY, char* str, uint16_t numChar, uint8_t textSize, uint16_t textColour, uint16_t backgroundColour) {
    FontDef font = Font_11x18;
    startX *= screen.resolutionScaling;
    startY *= screen.resolutionScaling;
    if ((startX >= SCREEN_WIDTH) || (startY >= SCREEN_HEIGHT)) {
        softAssert(0, "");
        return;
    }
    if ((font.height * textSize) >= (SCREEN_HEIGHT - startY)) {
        softAssert(0, "");
        return;
    }

    uint16_t littleEndianTextColour = ((textColour & 0xff00) >> 8) | ((textColour & 0xff) << 8);
    uint16_t littleEndianBGColour = ((backgroundColour & 0xff00) >> 8) | ((backgroundColour & 0xff) << 8);

    // Find length of each line
    #define MAX_NUM_LINES 10
    uint16_t lengths[MAX_NUM_LINES] = {};
    uint8_t lines = 0;
    uint16_t maxLength = 0;
    for (uint16_t i=0; i<numChar; i++) {
        if (str[i] == '\0') {
            lines++;
            break;
        }
        if (str[i] == '\n') {
            lines++;
            // End if too many lines
            if (lines >= MAX_NUM_LINES) {
                break;
            }
            if (((lines +1) * font.height * textSize) >= (SCREEN_HEIGHT - startY)) {
                break;
            }
        } else {
            if (((lengths[lines]+1) * textSize * font.width) < (SCREEN_WIDTH - startX)) {
                lengths[lines]++;
                maxLength = MAX(maxLength, lengths[lines]);
            }
            // Ignore character if too long
        }
    }

    uint16_t width = font.width * textSize * maxLength;
    uint16_t height = font.height * textSize * lines;
    hardAssert(width < SCREEN_WIDTH, "");
    hardAssert(height < SCREEN_HEIGHT, "");
    tftSetAddressWindow(startX, startY, startX + width - 1, startY + height - 1);

    char * tmpStr = str;
    for (uint8_t l=0; l<lines; l++) {
        // Deal with any line length differences
        uint16_t diff = (maxLength - lengths[l]) * font.width * textSize;

        // Loop through all rows that make up the font
        for(uint32_t y=0; y<font.height; y++) {
            uint16_t * dmaPtr = (uint16_t *) screenDmaBuffer;

            // Loop through all the characters in the line
            for (uint8_t ci=0; ci<lengths[l]; ci++) {
                uint8_t ch = tmpStr[ci];
                uint16_t row = font.data[(ch - 32) * font.height + y];

                // Set a row of the character
                for (uint16_t z=0; z<font.width; z++) {
                    uint16_t colour = (row & 0x8000) ? littleEndianTextColour : littleEndianBGColour;
                    for (uint8_t j=0; j<textSize; j++) {
                        *dmaPtr = colour;
                        dmaPtr++;
                    }
                    row = row << 1;
                }
            }

            // Set line length difference pixels in row
            for (uint16_t i=0; i<diff; i++) {
                *dmaPtr = littleEndianBGColour;
                dmaPtr++;
            }
            hardAssert(((uint8_t *) dmaPtr) < (screenDmaBuffer + DMA_BUFFER_SIZE_BYTES), "");

            // Write row (multiples times if the text size is > 1)
            for (uint8_t i=0; i<textSize; i++) {
                tftWriteDataDma(screenDmaBuffer, 2*width);
                waitForDma();
            }
        }
        tmpStr += lengths[l] + 1;
        hardAssert(tmpStr <= (str + numChar), "");
    }
    finishDmaWrite();
}

// ========================================= //

void drawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t colour) {
    x *= screen.resolutionScaling;
    y *= screen.resolutionScaling;
    width *= screen.resolutionScaling;
    height *= screen.resolutionScaling;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;
    width = MIN(width, (SCREEN_WIDTH-x));
    height = MIN(height, (SCREEN_HEIGHT-y));
    if (width == 0 || height == 0)
        return;

    uint16_t littleEndianColour = ((colour & 0xff00) >> 8) | ((colour & 0xff) << 8);

    for (uint16_t p=0; p<width; p++) {
        ((uint16_t *) screenDmaBuffer)[p] = littleEndianColour;
    }
    tftSetAddressWindow(x, y, x+width-1, y+height-1);
    for (uint16_t i=0; i<height; i++) {
        tftWriteDataDma(screenDmaBuffer, 2*width);
        waitForDma();
    }
    waitForDma();
    finishDmaWrite();
}

// ========================================= //

void finishPixelStreaming() {
    finishDmaWrite();
    memset(&screen.pixelStream, 0, sizeof(screen.pixelStream));
}

void startPixelStreaming() {
    softAssert(!screen.pixelStream.running, "");
    screen.pixelStream.running = 1;
    if (screen.pixelWindow.width == 0 || screen.pixelWindow.height == 0)
    	return;
    uint8_t scaling = screen.resolutionScaling;
    uint16_t startX = screen.pixelWindow.x * scaling;
    uint16_t startY = screen.pixelWindow.y * scaling;
    uint16_t endX = startX + (screen.pixelWindow.width  * scaling) - 1;
    uint16_t endY = startY + (screen.pixelWindow.height * scaling) - 1;
    // These asserts should never occur because the checks should be done before the command goes into the buffer
    softAssert(startX < SCREEN_WIDTH, "Invalid pixel stream start x");
    softAssert(startY < SCREEN_HEIGHT, "Invalid pixel stream start y");
    softAssert(endX < SCREEN_WIDTH, "Invalid pixel stream end x");
    softAssert(endY < SCREEN_HEIGHT, "Invalid pixel stream end y");
    tftSetAddressWindow(startX, startY, endX, endY);
}

void streamPixels(uint16_t numNewPixels) {
    uint16_t scaling = screen.resolutionScaling;
    tPixelWindow * w = &screen.pixelWindow;
    tPixelStream * stream = &screen.pixelStream;

    // Discard any pixels if not actually running
    if (!screen.pixelStream.running || w->width == 0 || w->height == 0) {
        popBuffer(&screen.dataBuffer, &screenDmaBuffer[0], numNewPixels*2);
        return;
    }

    while (numNewPixels) {
        // Work out how many pixels to put into the DMA buffer
        uint16_t dmaPixelsLeft = w->width - stream->rowPixelIndex;
        uint16_t numToCopy = MIN(dmaPixelsLeft, numNewPixels);
        numNewPixels -= numToCopy;

        uint8_t * dma = &screenDmaBuffer[stream->rowPixelIndex * scaling * 2];

        // Need to duplicate each pixel to make row
        hardAssert(numToCopy <= SCREEN_WIDTH/2, "");
        uint16_t tmpPixels[SCREEN_WIDTH/2]; // TODO: quite a lot for the stack
        popBuffer(&screen.dataBuffer, (uint8_t *) tmpPixels, numToCopy*2);
        uint16_t * pixels = tmpPixels;
        
        if (scaling == 1) {
            for (uint16_t p=0; p<numToCopy; p++) {
                uint16_t pixelColour = (*pixels) >> 8 | (*pixels) << 8;
                *((uint16_t *) dma) = pixelColour;
                dma += 2;
                pixels++;
            }

        } else {
            for (uint16_t p=0; p<numToCopy; p++) {
                uint16_t pixelColour = (*pixels) >> 8 | (*pixels) << 8;
                for (uint8_t r=0; r<scaling; r++) {
                    *((uint16_t *) dma) = pixelColour;
                    dma += 2;
                }
                pixels++;
            }
        }
        stream->rowPixelIndex += numToCopy;
        // Write the row
        if (stream->rowPixelIndex == w->width) {
            stream->rowPixelIndex = 0;
            waitForDma();
            for (uint8_t i=0; i<scaling; i++) {
                tftWriteDataDma(screenDmaBuffer, 2 * w->width * scaling);
                waitForDma();
            }
        }
    }
}

// ========================================= //

// Running in background thread so it doesn't matter that we are waiting on the dma all the time
void processActions() {
    if (BUFFER_EMPTY(screen.commandBuffer)) {
        return;
    }
//    softAssert(!dmaBusy(), "Always wait for DMA to finish");
    while (dmaBusy()) {
    	;
    }

    tCommandWord cmdWord = *(BUFFER_PEEK(screen.commandBuffer));
    BUFFER_POP(screen.commandBuffer);
    uint8_t command = cmdWord.command;
    uint8_t dataLength = cmdWord.length;
    softAssert(screen.dataBuffer.start == cmdWord.dataBufferStart, "data buffer start doesn't match the command ptr");
    screen.dataBuffer.start = cmdWord.dataBufferStart;

    // If non streaming command comes in whilst still streaming - kill the stream
    if ((command != STREAM_PIXELS_COMMAND && command != STOP_STREAM_PIXELS_COMMAND) && screen.pixelStream.running) {
        //softAssert(0, "Pixel streaming interrupted");
        finishPixelStreaming();
    }

    // Get new actions
    if (command == SET_PIXEL_WINDOW_COMMAND) {
        popBuffer(&screen.dataBuffer, (uint8_t *) &screen.pixelWindow, dataLength);

    } else if (command == START_STREAM_PIXELS_COMMAND) {
        startPixelStreaming();

    } else if (command == STOP_STREAM_PIXELS_COMMAND) {
        finishPixelStreaming();

    } else if (command == STREAM_PIXELS_COMMAND) {
        softAssert(screen.pixelStream.running, "Pixel streaming not started");
        streamPixels(dataLength/2);

    } else if (command == DRAW_RECTANGLE_COMMAND) {
        tDrawRectangle action;
        popBuffer(&screen.dataBuffer, (uint8_t *) &action, dataLength);
        drawRectangle(action.x, action.y, action.width, action.height, action.colour);

    } else if (command == DRAW_TEXT_COMMAND) {
        tDrawText action;
        char text[NUM_CHARACTERS];
        popBuffer(&screen.dataBuffer, (uint8_t *) &action, sizeof(tDrawText));
        softAssert(dataLength == sizeof(tDrawText) + action.numChar, "");
        softAssert(action.numChar < NUM_CHARACTERS, "String too long");
        popBuffer(&screen.dataBuffer, (uint8_t *) text, action.numChar);
        drawText(action.x, action.y, text, action.numChar, action.size, action.colour, action.backgroundColour);

    } else if (command == CHANGE_RESOLUTION_SCALING) {
        popBuffer(&screen.dataBuffer, &screen.resolutionScaling, dataLength);

    } else {
        softAssert(0, "Invalid command");
    }
}

// ========================================= //


void refreshAndPublishCmdSpaceRemaining() {
    screen.cmdSpaceRemaining = DATA_BUFFER_BYTES - bufferLength(&screen.dataBuffer);
    field_t fieldIndex = SCREEN_FIELDS_OFFSET + 15 + NUM_CHARACTERS;
    publishFieldsIfBelowBandwidth(fieldIndex, fieldIndex);
}

void storeCommand(uint8_t command, uint16_t size) {
    tCommandWord cmdWord;
    cmdWord.command = command;
    cmdWord.dataBufferStart = screen.dataBuffer.end;
    cmdWord.length = size;
    BUFFER_APPEND(screen.commandBuffer, cmdWord);
}

void storeData(uint8_t * data, uint16_t size) {
    if (size > 0) {
        appendBuffer(&screen.dataBuffer, data, size);
    }
}

void storeCommandAndData(uint8_t command, void * data, uint16_t size) {
    storeCommand(command, size);
    storeData(data, size);
}

// ========================================= //

void setResolutionScaling(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    if (*data > 0 && *data <= SCREEN_HEIGHT) {
        screen.newResolutionScaling = *data;
        storeCommandAndData(CHANGE_RESOLUTION_SCALING, &screen.newResolutionScaling, sizeof(screen.newResolutionScaling));
    }
}

void setDrawRectangle(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    storeCommandAndData(DRAW_RECTANGLE_COMMAND, &screen.drawRectangle, sizeof(tDrawRectangle));
    refreshAndPublishCmdSpaceRemaining();
}

void setDrawText(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    screen.text[NUM_CHARACTERS-1] = '\0'; // Make sure it always ends in terminating char
    screen.drawText.numChar = strlen(screen.text)+1;
    storeCommand(DRAW_TEXT_COMMAND, sizeof(tDrawText) + screen.drawText.numChar);
    storeData((uint8_t *) &screen.drawText, sizeof(tDrawText));
    storeData((uint8_t *) screen.text, screen.drawText.numChar);
    refreshAndPublishCmdSpaceRemaining();
}

void setStopPixelStreaming(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    storeCommand(STOP_STREAM_PIXELS_COMMAND, 0);
}

void setStartPixelStreaming(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    // Check if window is valid
    tPixelWindow * w = &screen.newPixelWindow;
    uint16_t scaling = screen.newResolutionScaling;
    uint32_t endX = (w->x + w->width) * scaling;
    uint32_t endY = (w->y + w->height) * scaling;
    if ((endX > SCREEN_WIDTH) || (endY > SCREEN_HEIGHT)) {
        w->x = 0;
        w->y = 0;
        w->width = 0;
        w->height = 0;
    }
    // Store a set window command if the window parameters have changed then
    static tPixelWindow lastWindow = {};
    if ((screen.newPixelWindow.x      != lastWindow.x) ||
        (screen.newPixelWindow.y      != lastWindow.y) ||
        (screen.newPixelWindow.width  != lastWindow.width) ||
        (screen.newPixelWindow.height != lastWindow.height)) {
        storeCommandAndData(SET_PIXEL_WINDOW_COMMAND,  w, sizeof(tPixelWindow));
        lastWindow = *w;
    }
    storeCommand(START_STREAM_PIXELS_COMMAND, 0);
}

void setPixelDataStream(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    uint16_t numPixels = endFieldIndex - startFieldIndex;
    storeCommand(STREAM_PIXELS_COMMAND, numPixels * 2);
    storeData(data, numPixels * 2);
    // refreshAndPublishCmdSpaceRemaining();
}

void setTextSize(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    if (*data > 0) {
        screen.drawText.size = *data;
    }
}

void setInvertColours(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    tftInvertColors(!(data[0]));
}

void setRotation(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
    tftSetRotation(data[0]);
}
// void setBacklightField(const sFieldInfoEntry * fieldInfo, field_t startFieldIndex, field_t endFieldIndex, uint8_t * data) {
//     setBacklight(data[0]);
// }

static uint8_t zero = 0;
static uint8_t one = 1;
static uint8_t * const screenName = (uint8_t *) "screen";
static sExtraMetaDataFields screenFieldMetaData        = {1, {{COMPONENT_NAME, screenName}}};
static sExtraMetaDataFields screenNonSettableMetaData  = {3, {{SETTABLE, &zero} , {DAEMON_ONLY_VISIBLE, &one}, {COMPONENT_NAME, screenName}}};

sFieldInfoEntry screenFieldTable[] = {
    { &screen.drawRectangle.x,               "rectangle_x",                 SCREEN_FIELDS_OFFSET +  0,                   1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawRectangle.y,               "rectangle_y",                 SCREEN_FIELDS_OFFSET +  1,                   1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawRectangle.width,           "rectangle_width",             SCREEN_FIELDS_OFFSET +  2,                   1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawRectangle.height,          "rectangle_height",            SCREEN_FIELDS_OFFSET +  3,                   1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawRectangle.colour,          "rectangle_colour",            SCREEN_FIELDS_OFFSET +  4,                   1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { NULL,                                  "rectangle_draw",              SCREEN_FIELDS_OFFSET +  5,                   1,                     AF_FIELD_TYPE_BOOLEAN,   1,   0, &screenFieldMetaData, &setDrawRectangle,   NULL },
    { &screen.text[0],                       "text",                        SCREEN_FIELDS_OFFSET +  6,                   NUM_CHARACTERS,        AF_FIELD_TYPE_UTF8_CHAR, 1,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawText.x,                    "text_x",                      SCREEN_FIELDS_OFFSET +  6 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawText.y,                    "text_y",                      SCREEN_FIELDS_OFFSET +  7 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawText.size,                 "text_size",                   SCREEN_FIELDS_OFFSET +  8 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      1,   0, &screenFieldMetaData, &setTextSize,        NULL },
    { &screen.drawText.colour,               "text_colour",                 SCREEN_FIELDS_OFFSET +  9 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.drawText.backgroundColour,     "text_background_colour",      SCREEN_FIELDS_OFFSET + 10 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { NULL,                                  "text_draw",                   SCREEN_FIELDS_OFFSET + 11 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_BOOLEAN,   1,   0, &screenFieldMetaData, &setDrawText,        NULL },
    { &screen.newResolutionScaling,          "resolution_scaling",          SCREEN_FIELDS_OFFSET + 12 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      1,   0, &screenFieldMetaData, &setResolutionScaling,  NULL },
    { &screen.invertColours,                 "invert_colours",              SCREEN_FIELDS_OFFSET + 13 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_BOOLEAN,   1,   0, &screenFieldMetaData, &setInvertColours,   NULL },
    { &screen.rotation,                      "rotation",                    SCREEN_FIELDS_OFFSET + 14 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      1,   0, &screenFieldMetaData, &setRotation,        NULL },
    { &screen.cmdSpaceRemaining,             "cmd_space_remaining",         SCREEN_FIELDS_OFFSET + 15 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      4,   0, &screenNonSettableMetaData, NULL,                NULL },
    { &screen.newPixelWindow.x,              "pixel_window_x",              SCREEN_FIELDS_OFFSET + 16 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.newPixelWindow.y,              "pixel_window_y",              SCREEN_FIELDS_OFFSET + 17 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.newPixelWindow.width,          "pixel_window_width",          SCREEN_FIELDS_OFFSET + 18 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { &screen.newPixelWindow.height,         "pixel_window_height",         SCREEN_FIELDS_OFFSET + 19 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_UINT,      2,   0, &screenFieldMetaData, NULL,                NULL },
    { NULL,                                  "start_pixel_streaming",       SCREEN_FIELDS_OFFSET + 20 + NUM_CHARACTERS,  1,                     AF_FIELD_TYPE_BOOLEAN,   1,   0, &screenFieldMetaData, &setStartPixelStreaming, NULL },
    { NULL,                                  "pixel_data_stream",           SCREEN_FIELDS_OFFSET + 21 + NUM_CHARACTERS,  MAX_PACKET_PIXELS,     AF_FIELD_TYPE_UINT,      2,   DONT_REPLY_TO_SET_FLAG, &screenFieldMetaData, &setPixelDataStream, NULL },
    { NULL,                                  "stop_pixel_streaming",        SCREEN_FIELDS_OFFSET + 21 + NUM_CHARACTERS + MAX_PACKET_PIXELS,  1, AF_FIELD_TYPE_BOOLEAN,   1,   0, &screenFieldMetaData, &setStopPixelStreaming, NULL },
    // /*  -  */ { &screen.backlightEnabled,                 "backlight",                   AF_FIELD_TYPE_BOOLEAN,   1,   1,                 },
    // {   "triangle_x1",                 1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleX1 },
    // {   "triangle_y1",                 1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleY1 },
    // {   "triangle_x2",                 1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleX2 },
    // {   "triangle_y2",                 1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleY2 },
    // {   "triangle_x3",                 1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleX3 },
    // {   "triangle_y3",                 1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleY3 },
    // {   "triangle_colour",             1,                               AF_FRAME_FIELD_TYPE_UINT16,  &screen.triangleColour },
    // {   "triangle_draw",               1,                               AF_FRAME_FIELD_TYPE_BOOL8,   NULL }, // Actually a commmand
};
const uint32_t screenFieldTableSize = sizeof(screenFieldTable)/sizeof(screenFieldTable[0]);


const sFieldInfoEntry * getFieldInfo(field_t fieldIndex);

void setBacklight(uint8_t enable) {
    HAL_GPIO_WritePin(screen.backlightGpioSection, screen.backlightGpioPin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


// void testPixelStreaming();

void initialiseScreen(GPIO_TypeDef* powerGpioSection,
                    uint16_t powerGpioPin, 
                    GPIO_TypeDef* csGpioSection,
                    uint16_t csGpioPin,
                    GPIO_TypeDef* dcGpioSection,
                    uint16_t dcGpioPin,
                    GPIO_TypeDef* resetGpioSection,
                    uint16_t resetGpioPin,
                    GPIO_TypeDef* backlightGpioSection,
                    uint16_t backlightGpioPin, 
                    SPI_HandleTypeDef * spi) {
    memset(screenDmaBuffer, 0, sizeof(screenDmaBuffer));
    addComponentFieldTableToGlobalTable(screenFieldTable, screenFieldTableSize);
    screen.backlightGpioSection = backlightGpioSection;
    screen.backlightGpioPin = backlightGpioPin;
    screen.backlightEnabled = 1;
    screen.resolutionScaling = 1;
    screen.newResolutionScaling = 1;
    screen.dataBuffer.data = screen.dataBufferData;
    screen.dataBuffer.size = sizeof(screen.dataBufferData);
    screen.commandBuffer.size = COMMAND_BUFFER_LENGTH;
    screen.cmdSpaceRemaining = DATA_BUFFER_BYTES;
    
    HAL_GPIO_WritePin(resetGpioSection, resetGpioPin, GPIO_PIN_RESET);
    
    if (powerGpioSection != NULL) {
        HAL_GPIO_WritePin(powerGpioSection, powerGpioPin, GPIO_PIN_SET);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(powerGpioSection, powerGpioPin, GPIO_PIN_RESET);
        // HAL_Delay(50);
    }
    
    HAL_Delay(100);

    setBacklight(screen.backlightEnabled);

    initTft(csGpioSection, csGpioPin, dcGpioSection, dcGpioPin,
            resetGpioSection, resetGpioPin, spi);

    tftInvertColors(1);

    {
        tDrawRectangle action = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK };
        screen.drawRectangle = action;
        setDrawRectangle(NULL, 0, 0, NULL);
    }

    {
        char text[] = "Starting...";
        tDrawText action = { 160-(11*13*2/2), 120-(18*2/2), 2, BLACK, WHITE, 0};
        screen.drawText = action;
        action.numChar = sizeof(text);
        memcpy(screen.text, text, action.numChar);
        setDrawText(NULL, 0, 0, NULL);
    }
}

void afterCanSetupScreen() {
    {
        tDrawRectangle action = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK };
        screen.drawRectangle = action;
        setDrawRectangle(NULL, 0, 0, NULL);
    }

    {
        char text[] = "Welcome";
        tDrawText action = { 160-(11*7*2/2), 120-(18*2/2), 2, WHITE, BLACK, 0};
        screen.drawText = action;
        action.numChar = sizeof(text);
        memcpy(screen.text, text, action.numChar);
        setDrawText(NULL, 0, 0, NULL);
    }
}

void loopScreen(uint32_t ticks) {
    static uint32_t lastTime = 0;

    if (ticks - lastTime > 200) {
        lastTime = ticks;
        refreshAndPublishCmdSpaceRemaining();
    }
    
	static uint32_t last5secondTime = 0;
	if ((ticks - last5secondTime) > 5000) {
		last5secondTime = ticks;
		publishFieldsIfBelowBandwidth(SCREEN_FIELDS_OFFSET, SCREEN_FIELDS_OFFSET +  4);
		publishFieldsIfBelowBandwidth(SCREEN_FIELDS_OFFSET +  6 + NUM_CHARACTERS, SCREEN_FIELDS_OFFSET + 19 + NUM_CHARACTERS);
	}
}

void loopBackgroundScreen() {
    processActions();
}

// void changeScreenSpiFreq() {
//     SPI_TypeDef * instance = tftDrv.spi->Instance;
//     SPI_InitTypeDef init = tftDrv.spi->Init;
//     softAssert(HAL_SPI_DeInit(tftDrv.spi) == HAL_OK, "");
//     tftDrv.spi->Instance = instance;
//     tftDrv.spi->Init = init;
//     // Change the SPI speed from 24 MHz to 3Mhz
//     tftDrv.spi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
//     softAssert(HAL_SPI_Init(tftDrv.spi) == HAL_OK, "");
//     initialiseScreen(
//         tftDrv.csGpioSection,
//         tftDrv.csGpioPin,
//         tftDrv.dcGpioSection,
//         tftDrv.dcGpioPin,
//         tftDrv.resetGpioSection,
//         tftDrv.resetGpioPin,
//         screen.backlightGpioSection,
//         screen.backlightGpioPin,
//         tftDrv.spi
//     );
// }


#endif //ENABLE_SCREEN_MODULE


// void testPixelStreaming() {
//     uint8_t tmp3 = 30;
//     setResolutionScaling(NULL, 0, 0, &tmp3);
    
    

//     // All good test
//     {
//         tPixelWindow tmp = {0,0,6,2};
//         screen.newPixelWindow = tmp;
//         setStartPixelStreaming(NULL, 0, 0, NULL);

//         uint16_t data[] = {
//             RED, YELLOW, RED, YELLOW, RED, YELLOW,
//             YELLOW, RED, YELLOW, RED, YELLOW, RED,
//         };
//         setPixelDataStream(NULL, 0, 2*6, (uint8_t *) data);
//         setStopPixelStreaming(NULL, 0, 0, NULL);
//     }

//     // Too little data
//     {
//         tPixelWindow tmp = {0,2,6,2};
//         screen.newPixelWindow = tmp;
//         setStartPixelStreaming(NULL, 0, 0, NULL);

//         uint16_t data[] = {
//             RED, YELLOW, GREEN, CYAN, BLUE, MAGENTA,
//             RED, YELLOW, GREEN, 
//         };
//         setPixelDataStream(NULL, 0, 2*6, (uint8_t *) data);
//         setStopPixelStreaming(NULL, 0, 0, NULL);
//     }

//     // Too much data
//     {
//         tPixelWindow tmp = {0,4,6,2};
//         screen.newPixelWindow = tmp;
//         setStartPixelStreaming(NULL, 0, 0, NULL);

//         uint16_t data[] = {
//             RED, YELLOW, GREEN, CYAN, BLUE, MAGENTA,
//             BLACK, YELLOW, BLACK, CYAN, BLACK, MAGENTA,
//             WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
//             WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
//             WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
//             WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
//         };
//         setPixelDataStream(NULL, 0, 36, (uint8_t *) data);
//         setStopPixelStreaming(NULL, 0, 0, NULL);
//     }

//     // Window too big
//     {
//         tPixelWindow tmp = {0,4,20,2};
//         screen.newPixelWindow = tmp;
//         setStartPixelStreaming(NULL, 0, 0, NULL);

//         uint16_t data[] = {
//             GREY, GREY, GREY, GREY, GREY, GREY,
//             GREY, GREY, GREY, GREY, GREY, GREY,
//             GREY, GREY, GREY, GREY, GREY, GREY,
//             GREY, GREY, GREY, GREY, GREY, GREY,
//             GREY, GREY, GREY, GREY, GREY, GREY,
//             GREY, GREY, GREY, GREY, GREY, GREY,
//         };
//         setPixelDataStream(NULL, 0, 36, (uint8_t *) data);
//         setStopPixelStreaming(NULL, 0, 0, NULL);
//     }


//     uint8_t tmp2 = 1;
//     setResolutionScaling(NULL, 0, 0, &tmp2);
    
//     while (bufferLength(&screen.dataBuffer)) {
//         processActions();
//     }
    
    

//     // Image
//     {
//         tPixelWindow tmp = {0,0,320,240};
//         screen.newPixelWindow = tmp;
//         setStartPixelStreaming(NULL, 0, 0, NULL);
        
//         uint32_t i=0;
//         uint16_t data[80];
//         for (uint32_t y=0; y<240; y++) {
//             for (uint32_t x=0; x<320; x++) {
//                 uint16_t colour = (((y + x)/2) % 64) << 5;
//                 data[i] = colour;
//                 i++;
//                 if (i == 80) {
//                 	i=0;
//                     setPixelDataStream(NULL, 0, 80, (uint8_t *) data);
//                     while (bufferLength(&screen.dataBuffer)) {
//                         processActions();
//                     }
//                 }
//             }
//         }
//         setStopPixelStreaming(NULL, 0, 0, NULL);
//     }
// }
