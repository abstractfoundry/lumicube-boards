/* Copyright (c) 2020 Abstract Foundry Limited */

#include "afException.h"
#include "string.h"
#include "useful.h"

char assertBuffer[ASSERT_BUFFER_SIZE] = {};
uint16_t assertBufferIndex = 0;
uint16_t assertCount = 0;

void softAssertTmp(char * msg) {
	assertCount++;
	if (ASSERT_BUFFER_SIZE > 1) {
		uint32_t length = strlen(msg);
		// Change last end of string to new line
		assertBuffer[assertBufferIndex] = '\n';
		// Copy message into buffer and add a newline
		length = MIN(length, ASSERT_BUFFER_SIZE - assertBufferIndex -1);
		memcpy(&assertBuffer[assertBufferIndex], msg, length);
		assertBufferIndex += length;
	}
}

void hardAssertTmp(char * msg) {
	softAssertTmp(msg);
	Throw(GENERAL_EXCEPTION);
}

