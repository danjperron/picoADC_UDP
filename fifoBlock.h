
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>
#include "picoADC_UDP.h"


#define BLOCK_MAX 140

#define BLOCK_FREE   1
#define BLOCK_READY  2
#define BLOCK_LOCK   4



extern SampleBlockStruct block[BLOCK_MAX];

// fifo pointer
extern uint16_t  head_block;
extern uint16_t  tail_block;

// fifo block head and tail pointer function
int getHeadBlock(uint16_t status);
int getTailBlock(uint16_t status);
int getTotalBlock(uint16_t status);
int getBlockId(uint32_t blockid, uint16_t status);

#endif

