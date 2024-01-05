
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>
#include "picoADC_SDCARD.h"




#define BLOCK_FREE   0
#define BLOCK_READY  1
#define BLOCK_LOCK   2



extern SampleBlockStruct block[BLOCK_MAX];

// fifo pointer
extern uint16_t  head_block;
extern uint16_t  tail_block;

// fifo block head and tail pointer function
int getHeadBlock();
int getTailBlock();
int nextHeadBlock();
int nextTailBlock();

#endif

