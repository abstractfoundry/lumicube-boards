/* Copyright (c) 2020 Abstract Foundry Limited */

#ifndef AF_EXCEPTION_H_
#define AF_EXCEPTION_H_

#include "CException.h"
#include "project.h"

#define GENERAL_EXCEPTION 1
#ifndef ASSERT_BUFFER_SIZE
	#define ASSERT_BUFFER_SIZE 50
#endif

extern char assertBuffer[];
extern uint16_t assertBufferIndex;
extern uint16_t assertCount;

void hardAssertTmp(char * msg);
void softAssertTmp(char * msg);

#define hardAssert(predicate, msg) { \
	if (!(predicate)) { \
		hardAssertTmp(msg); \
	} \
}
#define softAssert(predicate, msg) { \
	if (!(predicate)) { \
		softAssertTmp(msg); \
	} \
}

#endif /* AF_EXCEPTION_H_ */

