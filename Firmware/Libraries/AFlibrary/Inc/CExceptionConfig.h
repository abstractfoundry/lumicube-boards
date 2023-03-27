/* Copyright (c) 2020 Abstract Foundry Limited */

#include "stdint.h"

#define CEXCEPTION_NUM_ID 4
#define CEXCEPTION_GET_ID   (threadStack[threadStackIndex]) 

#define BACKGROUND_THREAD_ID 0
#define MAIN_THREAD_ID 1
extern uint8_t threadStack[];
extern uint8_t threadStackIndex;

// Need to be keep thread safe!
#define START_THREAD_EXCEPTION(threadId) { \
	softAssert(threadStackIndex < CEXCEPTION_NUM_ID, "Exceeded exception stack"); \
	threadStackIndex++; \
	threadStack[threadStackIndex] = (threadId); \
}
#define END_THREAD_EXCEPTION() { \
	softAssert(threadStackIndex > 0, "Underflowed exception stack"); \
	threadStackIndex--; \
}
